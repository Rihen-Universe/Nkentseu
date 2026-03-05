#pragma once

// =============================================================================
// NkSystemEvent.h
// Hiérarchie des événements système de bas niveau.
//
// Catégorie : NK_CAT_SYSTEM
//
// Enumerations :
//   NkPowerState         — état du système d'alimentation
//   NkDisplayChange      — type de changement d'affichage
//   NkMemoryPressure     — niveau de pression mémoire RAM
//
// Structures :
//   NkDisplayInfo        — informations sur un moniteur
//
// Classes :
//   NkSystemEvent                 — base abstraite (catégorie SYSTEM)
//     NkSystemPowerEvent          — alimentation / veille / réveil
//     NkSystemLocaleEvent         — changement de langue/région
//     NkSystemDisplayEvent        — moniteur ajouté/retiré/reconfiguré
//     NkSystemMemoryEvent         — pression mémoire RAM (mobile)
//     NkSystemTimeZoneEvent       — changement de fuseau horaire
//     NkSystemThemeEvent          — changement de thème OS (clair/sombre)
//     NkSystemAccessibilityEvent  — changement des réglages d'accessibilité
// =============================================================================

#include "NkEvent.h"
#include <cstring>
#include <string>

namespace nkentseu {

    // =========================================================================
    // NkPowerState — état du système d'alimentation
    // =========================================================================

    enum class NkPowerState : NkU32 {
        NK_POWER_NORMAL             = 0, ///< Fonctionnement normal
        NK_POWER_LOW_BATTERY,            ///< Batterie faible (avertissement)
        NK_POWER_CRITICAL_BATTERY,       ///< Batterie critique (< 5%)
        NK_POWER_PLUGGED_IN,             ///< Alimentation secteur connectée
        NK_POWER_UNPLUGGED,              ///< Passage sur batterie
        NK_POWER_SUSPEND,                ///< Mise en veille imminente
        NK_POWER_RESUME,                 ///< Réveil de veille
        NK_POWER_HIBERNATE,              ///< Hibernation imminente
        NK_POWER_SHUTDOWN,               ///< Arrêt du système iminent
        NK_POWER_MAX
    };

    inline const char* NkPowerStateToString(NkPowerState s) noexcept {
        switch (s) {
            case NkPowerState::NK_POWER_NORMAL:           return "NORMAL";
            case NkPowerState::NK_POWER_LOW_BATTERY:      return "LOW_BATTERY";
            case NkPowerState::NK_POWER_CRITICAL_BATTERY: return "CRITICAL_BATTERY";
            case NkPowerState::NK_POWER_PLUGGED_IN:       return "PLUGGED_IN";
            case NkPowerState::NK_POWER_UNPLUGGED:        return "UNPLUGGED";
            case NkPowerState::NK_POWER_SUSPEND:          return "SUSPEND";
            case NkPowerState::NK_POWER_RESUME:           return "RESUME";
            case NkPowerState::NK_POWER_HIBERNATE:        return "HIBERNATE";
            case NkPowerState::NK_POWER_SHUTDOWN:         return "SHUTDOWN";
            default:                                       return "UNKNOWN";
        }
    }

    // =========================================================================
    // NkDisplayChange — type de changement d'affichage
    // =========================================================================

    enum class NkDisplayChange : NkU32 {
        NK_DISPLAY_ADDED            = 0, ///< Nouveau moniteur connecté
        NK_DISPLAY_REMOVED,              ///< Moniteur déconnecté
        NK_DISPLAY_RESOLUTION_CHANGED,   ///< Résolution ou fréquence changée
        NK_DISPLAY_ORIENTATION_CHANGED,  ///< Orientation (portrait/paysage)
        NK_DISPLAY_DPI_CHANGED,          ///< Facteur d'échelle DPI changé
        NK_DISPLAY_PRIMARY_CHANGED,      ///< Moniteur principal changé
        NK_DISPLAY_MAX
    };

    // =========================================================================
    // NkMemoryPressure — niveau de pression RAM
    // =========================================================================

    enum class NkMemoryPressure : NkU32 {
        NK_MEM_NORMAL   = 0, ///< RAM suffisante
        NK_MEM_MODERATE,     ///< Pression modérée : libérer les caches non critiques
        NK_MEM_CRITICAL,     ///< Pression critique : libérer tout le non-essentiel
        NK_MEM_MAX
    };

    inline const char* NkMemoryPressureToString(NkMemoryPressure p) noexcept {
        switch (p) {
            case NkMemoryPressure::NK_MEM_NORMAL:   return "NORMAL";
            case NkMemoryPressure::NK_MEM_MODERATE: return "MODERATE";
            case NkMemoryPressure::NK_MEM_CRITICAL: return "CRITICAL";
            default:                                  return "UNKNOWN";
        }
    }

    // =========================================================================
    // NkDisplayInfo — informations sur un moniteur
    // =========================================================================

    /**
     * @brief Décrit les caractéristiques d'un moniteur physique.
     */
    struct NkDisplayInfo {
        NkU32 index       = 0;       ///< Index système (0 = moniteur principal)
        NkU32 width       = 0;       ///< Résolution horizontale [pixels logiques]
        NkU32 height      = 0;       ///< Résolution verticale [pixels logiques]
        NkU32 physWidth   = 0;       ///< Résolution horizontale [pixels physiques]
        NkU32 physHeight  = 0;       ///< Résolution verticale [pixels physiques]
        NkU32 refreshRate = 60;      ///< Taux de rafraîchissement [Hz]
        NkF32 dpiScale    = 1.f;     ///< Facteur d'échelle (1.0 = 100%, 1.5 = 150%)
        NkF32 dpiX        = 96.f;    ///< DPI horizontal physique
        NkF32 dpiY        = 96.f;    ///< DPI vertical physique
        NkI32 posX        = 0;       ///< Position X dans l'espace moniteurs virtuel
        NkI32 posY        = 0;       ///< Position Y dans l'espace moniteurs virtuel
        bool  isPrimary   = false;   ///< Moniteur principal ?
        char  name[64]    = {};      ///< Nom lisible (ex: "DELL U2720Q")

        NkDisplayInfo() { std::memset(name, 0, sizeof(name)); }
    };

    // =========================================================================
    // NkSystemEvent — base abstraite pour tous les événements système
    // =========================================================================

    /**
     * @brief Classe de base pour les événements de bas niveau du système
     *        d'exploitation.
     *
     * Catégorie : NK_CAT_SYSTEM
     */
    class NkSystemEvent : public NkEvent {
    public:
        NK_EVENT_CATEGORY_FLAGS(NkEventCategory::NK_CAT_SYSTEM)

    protected:
        explicit NkSystemEvent(NkU64 windowId = 0) noexcept : NkEvent(windowId) {}
    };

    // =========================================================================
    // NkSystemPowerEvent — alimentation, veille, réveil
    // =========================================================================

    /**
     * @brief Émis lors des transitions d'état du système d'alimentation.
     *
     * Pour NK_POWER_SUSPEND et NK_POWER_HIBERNATE, l'application doit
     * sauvegarder son état avant que le handler retourne, car l'OS peut
     * suspendre l'exécution immédiatement après.
     *
     * batteryLevel : [0,1] ou -1 si branché sur secteur / inconnu.
     */
    class NkSystemPowerEvent final : public NkSystemEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_SYSTEM_POWER)

        /**
         * @param state        Nouvel état d'alimentation.
         * @param batteryLevel Niveau de batterie [0,1] ou -1.
         * @param pluggedIn    true si branché sur secteur.
         * @param windowId     Identifiant de la fenêtre (0 = niveau OS).
         */
        NkSystemPowerEvent(NkPowerState state,
                            NkF32 batteryLevel = -1.f,
                            bool  pluggedIn    = false,
                            NkU64 windowId     = 0) noexcept
            : NkSystemEvent(windowId)
            , mState(state)
            , mBatteryLevel(batteryLevel)
            , mPluggedIn(pluggedIn)
        {}

        NkEvent*    Clone()    const override { return new NkSystemPowerEvent(*this); }
        std::string ToString() const override {
            return std::string("SystemPower(") + NkPowerStateToString(mState)
                 + (mBatteryLevel >= 0.f
                     ? " bat=" + std::to_string(static_cast<int>(mBatteryLevel * 100)) + "%"
                     : " wired")
                 + ")";
        }

        NkPowerState GetState()        const noexcept { return mState; }
        NkF32        GetBatteryLevel() const noexcept { return mBatteryLevel; }
        bool         IsPluggedIn()     const noexcept { return mPluggedIn; }
        bool         IsSuspending()    const noexcept {
            return mState == NkPowerState::NK_POWER_SUSPEND
                || mState == NkPowerState::NK_POWER_HIBERNATE
                || mState == NkPowerState::NK_POWER_SHUTDOWN;
        }
        bool IsResuming() const noexcept {
            return mState == NkPowerState::NK_POWER_RESUME;
        }

    private:
        NkPowerState mState        = NkPowerState::NK_POWER_NORMAL;
        NkF32        mBatteryLevel = -1.f;
        bool         mPluggedIn   = false;
    };

    // =========================================================================
    // NkSystemLocaleEvent — changement de langue / région
    // =========================================================================

    /**
     * @brief Émis lorsque l'utilisateur change la langue ou la région dans les
     *        paramètres système.
     *
     * Les chaînes de locale suivent la convention POSIX (ex: "fr_FR", "en_US",
     * "ja_JP") ou BCP-47 (ex: "fr-FR").
     */
    class NkSystemLocaleEvent final : public NkSystemEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_SYSTEM_LOCALE)

        /**
         * @param locale     Nouvelle locale (ex: "fr_FR").
         * @param prevLocale Locale précédente (vide si inconnue).
         * @param windowId   Identifiant de la fenêtre (0 = niveau OS).
         */
        NkSystemLocaleEvent(const char* locale,
                             const char* prevLocale = "",
                             NkU64       windowId   = 0) noexcept
            : NkSystemEvent(windowId)
        {
            std::strncpy(mLocale,     locale,     sizeof(mLocale) - 1);
            std::strncpy(mPrevLocale, prevLocale, sizeof(mPrevLocale) - 1);
        }

        NkEvent*    Clone()    const override { return new NkSystemLocaleEvent(*this); }
        std::string ToString() const override {
            return std::string("SystemLocale(") + mPrevLocale + " -> " + mLocale + ")";
        }

        const char* GetLocale()     const noexcept { return mLocale; }
        const char* GetPrevLocale() const noexcept { return mPrevLocale; }

    private:
        char mLocale[48]     = {};
        char mPrevLocale[48] = {};
    };

    // =========================================================================
    // NkSystemDisplayEvent — moniteur ajouté / retiré / reconfiguré
    // =========================================================================

    /**
     * @brief Émis lors d'un changement dans la configuration des moniteurs.
     *
     * Peut survenir lors du branchement / débranchement d'un moniteur externe,
     * d'un changement de résolution, de DPI ou d'orientation.
     */
    class NkSystemDisplayEvent final : public NkSystemEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_SYSTEM_DISPLAY)

        /**
         * @param change     Type de changement survenu.
         * @param info       Informations sur le moniteur concerné.
         * @param windowId   Identifiant de la fenêtre (0 = niveau OS).
         */
        NkSystemDisplayEvent(NkDisplayChange    change,
                              const NkDisplayInfo& info,
                              NkU64 windowId = 0) noexcept
            : NkSystemEvent(windowId)
            , mChange(change)
            , mInfo(info)
        {}

        NkEvent*    Clone()    const override { return new NkSystemDisplayEvent(*this); }
        std::string ToString() const override {
            const char* changes[] = {
                "Added", "Removed", "Resolution", "Orientation", "DPI", "Primary"
            };
            NkU32 ci = static_cast<NkU32>(mChange);
            return std::string("SystemDisplay(#") + std::to_string(mInfo.index)
                 + " " + (ci < 6 ? changes[ci] : "?")
                 + " " + std::to_string(mInfo.width) + "x" + std::to_string(mInfo.height) + ")";
        }

        NkDisplayChange      GetChange() const noexcept { return mChange; }
        const NkDisplayInfo& GetInfo()   const noexcept { return mInfo; }

        bool IsAdded()           const noexcept { return mChange == NkDisplayChange::NK_DISPLAY_ADDED; }
        bool IsRemoved()         const noexcept { return mChange == NkDisplayChange::NK_DISPLAY_REMOVED; }
        bool IsResolutionChange()const noexcept { return mChange == NkDisplayChange::NK_DISPLAY_RESOLUTION_CHANGED; }
        bool IsDpiChange()       const noexcept { return mChange == NkDisplayChange::NK_DISPLAY_DPI_CHANGED; }

    private:
        NkDisplayChange mChange;
        NkDisplayInfo   mInfo;
    };

    // =========================================================================
    // NkSystemMemoryEvent — pression mémoire RAM (mobile / système embarqué)
    // =========================================================================

    /**
     * @brief Émis lorsque le système signale une pression sur la mémoire RAM.
     *
     * Particulièrement important sur iOS (memory warning), Android (onLowMemory /
     * onTrimMemory) et les systèmes embarqués.
     * L'application doit libérer les ressources non essentielles (caches d'assets,
     * LODs de haute résolution, audio non joué...).
     */
    class NkSystemMemoryEvent final : public NkSystemEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_SYSTEM_MEMORY)

        /**
         * @param pressure       Niveau de pression mémoire.
         * @param availableBytes Mémoire disponible [octets] (0 = inconnu).
         * @param totalBytes     Mémoire totale du système [octets].
         * @param windowId       Identifiant de la fenêtre (0 = niveau OS).
         */
        NkSystemMemoryEvent(NkMemoryPressure pressure,
                             NkU64 availableBytes = 0,
                             NkU64 totalBytes     = 0,
                             NkU64 windowId       = 0) noexcept
            : NkSystemEvent(windowId)
            , mPressure(pressure)
            , mAvailableBytes(availableBytes)
            , mTotalBytes(totalBytes)
        {}

        NkEvent*    Clone()    const override { return new NkSystemMemoryEvent(*this); }
        std::string ToString() const override {
            return std::string("SystemMemory(") + NkMemoryPressureToString(mPressure)
                 + (mAvailableBytes > 0
                    ? " avail=" + std::to_string(mAvailableBytes / (1024*1024)) + "MB"
                    : "")
                 + ")";
        }

        NkMemoryPressure GetPressure()       const noexcept { return mPressure; }
        NkU64            GetAvailableBytes() const noexcept { return mAvailableBytes; }
        NkU64            GetTotalBytes()     const noexcept { return mTotalBytes; }
        bool             IsCritical()        const noexcept { return mPressure == NkMemoryPressure::NK_MEM_CRITICAL; }

        /// @brief Mémoire disponible en Mo (0 si inconnu)
        NkU64 GetAvailableMb() const noexcept { return mAvailableBytes / (1024ULL * 1024ULL); }

    private:
        NkMemoryPressure mPressure       = NkMemoryPressure::NK_MEM_NORMAL;
        NkU64            mAvailableBytes = 0;
        NkU64            mTotalBytes     = 0;
    };

    // =========================================================================
    // NkSystemTimeZoneEvent — changement de fuseau horaire
    // =========================================================================

    /**
     * @brief Émis lorsque le fuseau horaire du système change.
     *
     * Peut nécessiter une mise à jour de l'affichage des heures et des
     * planifications basées sur l'heure locale.
     *
     * Les identifiants suivent la base de données IANA (ex: "Europe/Paris",
     * "America/New_York", "UTC").
     */
    class NkSystemTimeZoneEvent final : public NkSystemEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_SYSTEM_LOCALE)  // proxy (pas de type NK_SYSTEM_TIMEZONE)

        /**
         * @param tzId      Identifiant IANA du nouveau fuseau (ex: "Europe/Paris").
         * @param prevTzId  Identifiant précédent (vide si inconnu).
         * @param offsetMin Décalage par rapport à UTC en minutes (ex: +60 pour UTC+1).
         * @param windowId  Identifiant de la fenêtre (0 = niveau OS).
         */
        NkSystemTimeZoneEvent(const char* tzId,
                               const char* prevTzId  = "",
                               NkI32       offsetMin = 0,
                               NkU64       windowId  = 0) noexcept
            : NkSystemEvent(windowId)
            , mOffsetMin(offsetMin)
        {
            std::strncpy(mTzId,     tzId,     sizeof(mTzId) - 1);
            std::strncpy(mPrevTzId, prevTzId, sizeof(mPrevTzId) - 1);
        }

        NkEvent*    Clone()    const override { return new NkSystemTimeZoneEvent(*this); }
        std::string ToString() const override {
            return std::string("SystemTimeZone(") + mPrevTzId + " -> " + mTzId
                 + " UTC" + (mOffsetMin >= 0 ? "+" : "") + std::to_string(mOffsetMin / 60) + ")";
        }

        const char* GetTimeZoneId()     const noexcept { return mTzId; }
        const char* GetPrevTimeZoneId() const noexcept { return mPrevTzId; }
        NkI32       GetOffsetMinutes()  const noexcept { return mOffsetMin; }

    private:
        char  mTzId[64]     = {};
        char  mPrevTzId[64] = {};
        NkI32 mOffsetMin    = 0;
    };

    // =========================================================================
    // NkSystemThemeEvent — changement de thème OS (clair / sombre / contraste)
    // =========================================================================

    /**
     * @brief Émis lorsque le thème système change.
     *
     * Permet à l'application de basculer entre thèmes clair/sombre sans
     * redémarrage, conformément aux guidelines macOS / Windows / iOS / Android.
     *
     * @note  Pour les thèmes de fenêtre individuelle, voir NkWindowThemeChangeEvent
     *        dans NkWindowEvents.h.
     */
    class NkSystemThemeEvent final : public NkSystemEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_SYSTEM_DISPLAY)  // proxy

        enum class Theme : NkU32 {
            Light       = 0,
            Dark,
            HighContrast
        };

        /**
         * @param theme    Nouveau thème système.
         * @param prevTheme Thème précédent.
         * @param accentR  Composante rouge de la couleur d'accentuation OS [0-255].
         * @param accentG  Composante verte.
         * @param accentB  Composante bleue.
         * @param windowId Identifiant de la fenêtre (0 = niveau OS).
         */
        NkSystemThemeEvent(Theme theme,
                            Theme prevTheme  = Theme::Light,
                            NkU8  accentR    = 0,
                            NkU8  accentG    = 120,
                            NkU8  accentB    = 215,
                            NkU64 windowId   = 0) noexcept
            : NkSystemEvent(windowId)
            , mTheme(theme)
            , mPrevTheme(prevTheme)
            , mAccentR(accentR)
            , mAccentG(accentG)
            , mAccentB(accentB)
        {}

        NkEvent*    Clone()    const override { return new NkSystemThemeEvent(*this); }
        std::string ToString() const override {
            const char* names[] = { "Light", "Dark", "HighContrast" };
            NkU32 ti = static_cast<NkU32>(mTheme);
            return std::string("SystemTheme(") + (ti < 3 ? names[ti] : "?") + ")";
        }

        Theme GetTheme()     const noexcept { return mTheme; }
        Theme GetPrevTheme() const noexcept { return mPrevTheme; }
        NkU8  GetAccentR()   const noexcept { return mAccentR; }
        NkU8  GetAccentG()   const noexcept { return mAccentG; }
        NkU8  GetAccentB()   const noexcept { return mAccentB; }
        bool  IsDark()        const noexcept { return mTheme == Theme::Dark; }
        bool  IsLight()       const noexcept { return mTheme == Theme::Light; }
        bool  IsHighContrast()const noexcept { return mTheme == Theme::HighContrast; }

    private:
        Theme mTheme     = Theme::Light;
        Theme mPrevTheme = Theme::Light;
        NkU8  mAccentR   = 0;
        NkU8  mAccentG   = 120;
        NkU8  mAccentB   = 215;
    };

    // =========================================================================
    // NkSystemAccessibilityEvent — changement des réglages d'accessibilité
    // =========================================================================

    /**
     * @brief Émis lorsqu'un paramètre d'accessibilité OS change.
     *
     * Permet à l'application d'adapter son rendu et son comportement aux
     * besoins de l'utilisateur (taille de police, réduction des animations...).
     */
    class NkSystemAccessibilityEvent final : public NkSystemEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_SYSTEM_DISPLAY)  // proxy

        /**
         * @param reduceMotion     true si "Réduire les animations" est activé.
         * @param increaseContrast true si "Augmenter le contraste" est activé.
         * @param invertColors     true si "Inverser les couleurs" est activé.
         * @param boldText         true si "Texte en gras" est activé.
         * @param largeText        true si taille de police augmentée.
         * @param fontScale        Facteur d'échelle de la police (1.0 = normal).
         * @param windowId         Identifiant de la fenêtre (0 = niveau OS).
         */
        NkSystemAccessibilityEvent(bool  reduceMotion     = false,
                                    bool  increaseContrast = false,
                                    bool  invertColors     = false,
                                    bool  boldText         = false,
                                    bool  largeText        = false,
                                    NkF32 fontScale        = 1.f,
                                    NkU64 windowId         = 0) noexcept
            : NkSystemEvent(windowId)
            , mReduceMotion(reduceMotion)
            , mIncreaseContrast(increaseContrast)
            , mInvertColors(invertColors)
            , mBoldText(boldText)
            , mLargeText(largeText)
            , mFontScale(fontScale)
        {}

        NkEvent*    Clone()    const override { return new NkSystemAccessibilityEvent(*this); }
        std::string ToString() const override {
            return std::string("SystemAccessibility(")
                 + (mReduceMotion    ? "reduceMotion "    : "")
                 + (mIncreaseContrast? "highContrast "    : "")
                 + (mInvertColors   ? "invertColors "    : "")
                 + "fontScale=" + std::to_string(mFontScale) + ")";
        }

        bool  ReduceMotion()     const noexcept { return mReduceMotion; }
        bool  IncreaseContrast() const noexcept { return mIncreaseContrast; }
        bool  InvertColors()     const noexcept { return mInvertColors; }
        bool  BoldText()         const noexcept { return mBoldText; }
        bool  LargeText()        const noexcept { return mLargeText; }
        NkF32 GetFontScale()     const noexcept { return mFontScale; }

    private:
        bool  mReduceMotion     = false;
        bool  mIncreaseContrast = false;
        bool  mInvertColors     = false;
        bool  mBoldText         = false;
        bool  mLargeText        = false;
        NkF32 mFontScale        = 1.f;
    };

} // namespace nkentseu