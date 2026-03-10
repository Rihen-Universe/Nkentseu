#pragma once

// =============================================================================
// NkGenericHidEvent.h
// Hiérarchie des événements pour les périphériques HID génériques.
//
// Catégorie : NK_CAT_GENERIC_HID
//
// Cette catégorie couvre tous les périphériques USB/BT HID qui ne sont pas
// des claviers, souris, manettes ou écrans tactiles -- exemples typiques :
//   - Volants de course et pédales
//   - Joysticks de simulation de vol
//   - Tablettes graphiques
//   - Contrôleurs MIDI
//   - Périphériques médicaux (manettes chirurgicales...)
//   - Périphériques industriels
//
// Enumerations :
//   NkHidUsagePage  — USB HID Usage Pages (top-level)
//
// Structures :
//   NkHidDeviceInfo — métadonnées d'un périphérique HID
//
// Classes :
//   NkGenericHidEvent               — base abstraite (catégorie GENERIC_HID)
//     NkHidConnectEvent             — périphérique connecté
//     NkHidDisconnectEvent          — périphérique déconnecté
//     NkHidButtonPressEvent         — bouton HID enfoncé
//     NkHidButtonReleaseEvent       — bouton HID relâché
//     NkHidAxisEvent                — axe analogique modifié
//     NkHidRawInputEvent            — rapport HID brut (données opaques)
// =============================================================================

#include "NkEvent.h"
#include "NkMouseEvent.h"  // pour NkButtonState
#include <cstring>
#include "NKContainers/String/NkStringUtils.h"
#include <vector>

namespace nkentseu {

    // =========================================================================
    // NkHidUsagePage — USB HID Usage Pages (selection des plus courants)
    // =========================================================================

    enum class NkHidUsagePage : NkU16 {
        NK_HID_PAGE_UNDEFINED     = 0x00,
        NK_HID_PAGE_GENERIC       = 0x01, ///< Generic Desktop Controls (joystick, gamepad...)
        NK_HID_PAGE_SIMULATION    = 0x02, ///< Simulation Controls (volant, avion...)
        NK_HID_PAGE_VR            = 0x03, ///< VR Controls
        NK_HID_PAGE_SPORT         = 0x04, ///< Sport Controls
        NK_HID_PAGE_GAME          = 0x05, ///< Game Controls
        NK_HID_PAGE_KEYBOARD      = 0x07, ///< Keyboard / Keypad
        NK_HID_PAGE_LED           = 0x08, ///< LEDs
        NK_HID_PAGE_BUTTON        = 0x09, ///< Button
        NK_HID_PAGE_DIGITIZER     = 0x0D, ///< Digitizer (tablette, stylet)
        NK_HID_PAGE_CONSUMER      = 0x0C, ///< Consumer Devices (télécommande, MIDI...)
        NK_HID_PAGE_SENSOR        = 0x20, ///< Sensors (accéléromètre, gyro...)
        NK_HID_PAGE_VENDOR_FIRST  = 0xFF00,
        NK_HID_PAGE_VENDOR_LAST   = 0xFFFF
    };

    // =========================================================================
    // NkHidDeviceInfo — métadonnées d'un périphérique HID
    // =========================================================================

    /**
     * @brief Informations sur un périphérique HID connecté.
     *
     * L'identifiant (deviceId) est attribué par le système et reste stable
     * tant que le périphérique est connecté à la même interface physique.
     */
    struct NkHidDeviceInfo {
        NkU64         deviceId   = 0;      ///< Identifiant opaque attribué par le système
        char          name[128]  = {};     ///< Nom lisible (ex: "Logitech G29 Racing Wheel")
        char          path[256]  = {};     ///< Chemin système (ex: /dev/input/event4)
        NkU16         vendorId   = 0;      ///< USB Vendor ID (VID)
        NkU16         productId  = 0;      ///< USB Product ID (PID)
        NkU16         usagePage  = 0;      ///< USB HID Usage Page primaire
        NkU16         usageId    = 0;      ///< USB HID Usage ID principal
        NkU32         numButtons = 0;      ///< Nombre de boutons déclarés
        NkU32         numAxes    = 0;      ///< Nombre d'axes analogiques
        NkU32         numHats    = 0;      ///< Nombre de chapeaux (D-pad HID)
        bool          isWireless = false;  ///< Connexion sans fil ?

        NkHidDeviceInfo() {
            std::memset(name, 0, sizeof(name));
            std::memset(path, 0, sizeof(path));
        }
    };

    // =========================================================================
    // NkGenericHidEvent — base abstraite pour tous les événements HID génériques
    // =========================================================================

    /**
     * @brief Classe de base pour les périphériques HID ne relevant pas des
     *        catégories spécialisées (clavier, souris, manette, tactile).
     *
     * Catégorie : NK_CAT_GENERIC_HID
     */
    class NkGenericHidEvent : public NkEvent {
    public:
        NK_EVENT_CATEGORY_FLAGS(NkEventCategory::NK_CAT_GENERIC_HID)

        /// @brief Identifiant opaque du périphérique HID source
        NkU64 GetDeviceId() const noexcept { return mDeviceId; }

    protected:
        NkGenericHidEvent(NkU64 deviceId,
                           NkU64 windowId = 0) noexcept
            : NkEvent(windowId)
            , mDeviceId(deviceId)
        {}

        NkU64 mDeviceId = 0;
    };

    // =========================================================================
    // NkHidConnectEvent — périphérique HID connecté
    // =========================================================================

    /**
     * @brief Émis lorsqu'un périphérique HID est détecté et initialisé.
     */
    class NkHidConnectEvent final : public NkGenericHidEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_GENERIC_CONNECT)

        explicit NkHidConnectEvent(const NkHidDeviceInfo& info,
                                    NkU64 windowId = 0) noexcept
            : NkGenericHidEvent(info.deviceId, windowId)
            , mInfo(info)
        {}

        NkEvent*    Clone()    const override { return new NkHidConnectEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("HidConnect({0} VID={1} PID={2})", mInfo.name, mInfo.vendorId, mInfo.productId);
        }

        const NkHidDeviceInfo& GetInfo() const noexcept { return mInfo; }

    private:
        NkHidDeviceInfo mInfo;
    };

    // =========================================================================
    // NkHidDisconnectEvent — périphérique HID déconnecté
    // =========================================================================

    /**
     * @brief Émis lorsqu'un périphérique HID est déconnecté.
     */
    class NkHidDisconnectEvent final : public NkGenericHidEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_GENERIC_DISCONNECT)

        NkHidDisconnectEvent(NkU64 deviceId,
                               NkU64 windowId = 0) noexcept
            : NkGenericHidEvent(deviceId, windowId)
        {}

        NkEvent*    Clone()    const override { return new NkHidDisconnectEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("HidDisconnect(device={0})", mDeviceId);
        }
    };

    // =========================================================================
    // NkHidButtonEvent — base interne pour press / release
    // =========================================================================

    /**
     * @brief Base interne pour les événements bouton HID.
     *
     * buttonIndex : index HID du bouton (0-based).
     * usagePage / usageId : identification HID standard.
     */
    class NkHidButtonEvent : public NkGenericHidEvent {
    public:
        NkU32         GetButtonIndex() const noexcept { return mButtonIndex; }
        NkButtonState GetState()       const noexcept { return mState; }
        NkU16         GetUsagePage()   const noexcept { return mUsagePage; }
        NkU16         GetUsageId()     const noexcept { return mUsageId; }

    protected:
        NkHidButtonEvent(NkU64         deviceId,
                          NkU32         buttonIndex,
                          NkButtonState state,
                          NkU16         usagePage,
                          NkU16         usageId,
                          NkU64         windowId) noexcept
            : NkGenericHidEvent(deviceId, windowId)
            , mButtonIndex(buttonIndex)
            , mState(state)
            , mUsagePage(usagePage)
            , mUsageId(usageId)
        {}

        NkU32         mButtonIndex = 0;
        NkButtonState mState       = NkButtonState::NK_RELEASED;
        NkU16         mUsagePage   = 0;
        NkU16         mUsageId     = 0;
    };

    // =========================================================================
    // NkHidButtonPressEvent
    // =========================================================================

    /**
     * @brief Émis lorsqu'un bouton HID est enfoncé.
     */
    class NkHidButtonPressEvent final : public NkHidButtonEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_GENERIC_BUTTON_PRESSED)

        /**
         * @param deviceId     Identifiant du périphérique.
         * @param buttonIndex  Index du bouton (0-based).
         * @param usagePage    Usage Page HID.
         * @param usageId      Usage ID HID.
         * @param windowId     Identifiant de la fenêtre.
         */
        NkHidButtonPressEvent(NkU64 deviceId,
                               NkU32 buttonIndex,
                               NkU16 usagePage = 0,
                               NkU16 usageId   = 0,
                               NkU64 windowId  = 0) noexcept
            : NkHidButtonEvent(deviceId, buttonIndex,
                                NkButtonState::NK_PRESSED,
                                usagePage, usageId, windowId)
        {}

        NkEvent*    Clone()    const override { return new NkHidButtonPressEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("HidButtonPress(dev={0} btn={1})", mDeviceId, mButtonIndex);
        }
    };

    // =========================================================================
    // NkHidButtonReleaseEvent
    // =========================================================================

    /**
     * @brief Émis lorsqu'un bouton HID est relâché.
     */
    class NkHidButtonReleaseEvent final : public NkHidButtonEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_GENERIC_BUTTON_RELEASED)

        NkHidButtonReleaseEvent(NkU64 deviceId,
                                  NkU32 buttonIndex,
                                  NkU16 usagePage = 0,
                                  NkU16 usageId   = 0,
                                  NkU64 windowId  = 0) noexcept
            : NkHidButtonEvent(deviceId, buttonIndex,
                                NkButtonState::NK_RELEASED,
                                usagePage, usageId, windowId)
        {}

        NkEvent*    Clone()    const override { return new NkHidButtonReleaseEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("HidButtonRelease(dev={0} btn={1})", mDeviceId, mButtonIndex);
        }
    };

    // =========================================================================
    // NkHidAxisEvent — axe analogique HID modifié
    // =========================================================================

    /**
     * @brief Émis lorsqu'un axe analogique change de valeur sur un périphérique HID.
     *
     * La valeur est normalisée à [-1.0, +1.0] pour les axes bidirectionnels
     * (sticks de volant, joystick) ou [0.0, +1.0] pour les axes unidirectionnels
     * (pédales, gâchettes).
     *
     * Le rawValue contient la valeur brute entière fournie par le pilote HID,
     * avant normalisation.
     */
    class NkHidAxisEvent final : public NkGenericHidEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_GENERIC_AXIS_MOTION)

        /**
         * @param deviceId    Identifiant du périphérique.
         * @param axisIndex   Index de l'axe (0-based).
         * @param value       Valeur normalisée [-1,1] ou [0,1].
         * @param prevValue   Valeur précédente.
         * @param rawValue    Valeur brute entière (avant normalisation).
         * @param usagePage   Usage Page HID de l'axe.
         * @param usageId     Usage ID HID de l'axe.
         * @param windowId    Identifiant de la fenêtre.
         */
        NkHidAxisEvent(NkU64 deviceId,
                        NkU32 axisIndex,
                        NkF32 value,
                        NkF32 prevValue,
                        NkI32 rawValue   = 0,
                        NkU16 usagePage  = 0,
                        NkU16 usageId    = 0,
                        NkU64 windowId   = 0) noexcept
            : NkGenericHidEvent(deviceId, windowId)
            , mAxisIndex(axisIndex)
            , mValue(value)
            , mPrevValue(prevValue)
            , mDelta(value - prevValue)
            , mRawValue(rawValue)
            , mUsagePage(usagePage)
            , mUsageId(usageId)
        {}

        NkEvent*    Clone()    const override { return new NkHidAxisEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("HidAxis(dev={0} axis={1} val={2:.3} delta={3:.3})",
                mDeviceId, mAxisIndex, mValue, mDelta);
        }

        NkU32 GetAxisIndex()  const noexcept { return mAxisIndex; }
        NkF32 GetValue()      const noexcept { return mValue; }
        NkF32 GetPrevValue()  const noexcept { return mPrevValue; }
        NkF32 GetDelta()      const noexcept { return mDelta; }
        NkI32 GetRawValue()   const noexcept { return mRawValue; }
        NkU16 GetUsagePage()  const noexcept { return mUsagePage; }
        NkU16 GetUsageId()    const noexcept { return mUsageId; }

    private:
        NkU32 mAxisIndex = 0;
        NkF32 mValue     = 0.f;
        NkF32 mPrevValue = 0.f;
        NkF32 mDelta     = 0.f;
        NkI32 mRawValue  = 0;
        NkU16 mUsagePage = 0;
        NkU16 mUsageId   = 0;
    };

    // =========================================================================
    // NkHidRawInputEvent — rapport HID brut
    // =========================================================================

    /**
     * @brief Émis avec le rapport HID brut tel que reçu du périphérique.
     *
     * Utilisé pour les périphériques non standard dont les données ne peuvent
     * pas être interprétées par le système d'entrée standard -- par exemple :
     * tablettes graphiques avec données de pression complexes, périphériques
     * médicaux, matériel industriel.
     *
     * reportId : identifiant du rapport HID (0 si non applicable).
     * data     : contenu du rapport (taille variable, dépend du descripteur HID).
     *
     * @note  L'interprétation des données nécessite la connaissance du
     *        descripteur HID du périphérique (vendorId + productId).
     */
    class NkHidRawInputEvent final : public NkGenericHidEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_GENERIC_BUTTON_PRESSED)  // proxy, même type

        /**
         * @param deviceId   Identifiant du périphérique.
         * @param reportId   Identifiant du rapport HID.
         * @param data       Données brutes du rapport (copiées).
         * @param windowId   Identifiant de la fenêtre.
         */
        NkHidRawInputEvent(NkU64             deviceId,
                             NkU8              reportId,
                             NkVector<NkU8> data,
                             NkU64             windowId = 0)
            : NkGenericHidEvent(deviceId, windowId)
            , mReportId(reportId)
            , mData(std::move(data))
        {}

        NkEvent*    Clone()    const override { return new NkHidRawInputEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("HidRawInput(dev={0} reportId={1} size={2}B)",
                mDeviceId, mReportId, mData.Size());
        }

        NkU8                     GetReportId() const noexcept { return mReportId; }
        const NkVector<NkU8>& GetData()     const noexcept { return mData; }
        const NkU8*              GetBytes()    const noexcept { return mData.Data(); }
        NkU32                    GetSize()     const noexcept { return static_cast<NkU32>(mData.Size()); }

    private:
        NkU8              mReportId = 0;
        NkVector<NkU8> mData;
    };

} // namespace nkentseu