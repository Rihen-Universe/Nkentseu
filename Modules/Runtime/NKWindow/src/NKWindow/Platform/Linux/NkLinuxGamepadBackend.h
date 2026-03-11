#pragma once

// =============================================================================
// NkLinuxGamepadBackend.h ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Joystick/Gamepad Linux via evdev
//
// Chemin : /dev/input/js* (interface joystick classique, read-only)
//   - Notifications inotify pour brancher ÃƒÆ’Ã‚Â  chaud
//   - Support rumble via /dev/input/event* si disponible (ff_effect)
//
// Compile pour NKENTSEU_WINDOWING_XCB / XLIB / WAYLAND.
// =============================================================================

#include "NKWindow/Events/NkGamepadSystem.h"
#include "NKMath/NkFunctions.h"
#include <cstring>
#include <cstdio>
#include <cmath>
#include <fcntl.h>
#include <unistd.h>
#include <linux/joystick.h>
#include <sys/inotify.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/input.h> // ff_effect, EVIOCGNAME

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

// ---------------------------------------------------------------------------
// Seuil deadzone par dÃƒÆ’Ã‚Â©faut pour les axes (jsDZ)
// ---------------------------------------------------------------------------

static constexpr float NK_JS_DEADZONE = 0.08f;

// ---------------------------------------------------------------------------
// NkJsDevice ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â un joystick /dev/input/jsN ouvert
// ---------------------------------------------------------------------------

struct NkJsDevice {
    int fd = -1;             ///< Descripteur du joystick
    int ffFd = -1;           ///< Descripteur event pour force-feedback
    int ffEffectId = -1;     ///< ID de l'effet rumble (ou -1)
    uint32 index = 0;         ///< Indice logique (0 = joueur 1ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦)
    char path[64] = {};      ///< Ex: "/dev/input/js0"
    char eventPath[64] = {}; ///< Ex: "/dev/input/event3" (ou vide)
    bool open = false;
};

// ---------------------------------------------------------------------------

class NkLinuxGamepad final : public NkIGamepad {
public:
    NkLinuxGamepad() = default;
    ~NkLinuxGamepad() override {
        Shutdown();
    }

    bool Init() override {
        for (auto &snap : mSnapshots)
            snap.Clear();
        for (auto &info : mInfos)
            info = {};
        for (auto &d : mDevices) {
            d.fd = -1;
            d.ffFd = -1;
            d.ffEffectId = -1;
            d.open = false;
        }

        // Surveillance /dev/input pour hot-plug
        mInotifyFd = inotify_init1(IN_NONBLOCK);
        if (mInotifyFd >= 0)
            inotify_add_watch(mInotifyFd, "/dev/input", IN_CREATE | IN_DELETE | IN_ATTRIB);

        ScanDevices();
        return true;
    }

    void Shutdown() override {
        for (auto &d : mDevices)
            CloseDevice(d);
        if (mInotifyFd >= 0) {
            close(mInotifyFd);
            mInotifyFd = -1;
        }
    }

    void Poll() override {
        // Hot-plug
        if (mInotifyFd >= 0)
            CheckHotPlug();

        // Lecture des ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements bruts
        for (uint32 i = 0; i < NK_MAX_GAMEPADS; ++i) {
            NkJsDevice &dev = mDevices[i];
            if (!dev.open || dev.fd < 0)
                continue;

            struct js_event evt{};
            while (read(dev.fd, &evt, sizeof(evt)) == sizeof(evt)) {
                // Ignore les ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements d'initialisation (JS_EVENT_INIT flag)
                uint8 type = evt.type & ~JS_EVENT_INIT;
                if (type == JS_EVENT_BUTTON) {
                    if (evt.number < static_cast<uint32>(NkGamepadButton::NK_GAMEPAD_BUTTON_MAX)) {
                        // Mapping gÃƒÆ’Ã‚Â©nÃƒÆ’Ã‚Â©rique : bouton js ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ NkGamepadButton
                        NkGamepadButton btn = JsButtonToNk(evt.number);
                        if (btn != NkGamepadButton::NK_GP_UNKNOWN)
                            mSnapshots[i].buttons[static_cast<uint32>(btn)] = (evt.value != 0);
                    }
                } else if (type == JS_EVENT_AXIS) {
                    // Axe js [-32767,32767] ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ [-1,1]
                    float v = static_cast<float>(evt.value) / 32767.f;
                    if (!math::NkIsFinite(v))
                        v = 0.f;
                    if (v > 1.f)
                        v = 1.f;
                    else if (v < -1.f)
                        v = -1.f;
                    if (math::NkFabs(v) < NK_JS_DEADZONE)
                        v = 0.f;
                    NkGamepadAxis ax = JsAxisToNk(evt.number);
                    if (ax != NkGamepadAxis::NK_GAMEPAD_AXIS_MAX)
                        mSnapshots[i].axes[static_cast<uint32>(ax)] = v;
                }
            }
        }
    }

    uint32 GetConnectedCount() const override {
        uint32 count = 0;
        for (uint32 i = 0; i < NK_MAX_GAMEPADS; ++i)
            if (mSnapshots[i].connected)
                ++count;
        return count;
    }

    const NkGamepadSnapshot& GetSnapshot(uint32 idx) const override {
        static NkGamepadSnapshot dummy;
        return idx < NK_MAX_GAMEPADS ? mSnapshots[idx] : dummy;
    }

    void Rumble(uint32 idx, float motorLow, float motorHigh, float /*tl*/, float /*tr*/, uint32 durationMs) override {
        if (idx >= NK_MAX_GAMEPADS)
            return;
        NkJsDevice &dev = mDevices[idx];
        if (dev.ffFd < 0)
            return;

        // ArrÃƒÆ’Ã‚Âªt de l'effet prÃƒÆ’Ã‚Â©cÃƒÆ’Ã‚Â©dent
        if (dev.ffEffectId >= 0) {
            ioctl(dev.ffFd, EVIOCRMFF, dev.ffEffectId);
            dev.ffEffectId = -1;
        }

        if (motorLow <= 0.f && motorHigh <= 0.f)
            return;

        struct ff_effect eff{};
        eff.type = FF_RUMBLE;
        eff.id = -1;
        eff.u.rumble.strong_magnitude = static_cast<uint16>(math::NkMin(motorLow, 1.f) * 0xFFFF);
        eff.u.rumble.weak_magnitude = static_cast<uint16>(math::NkMin(motorHigh, 1.f) * 0xFFFF);
        eff.replay.length = durationMs > 0 ? static_cast<uint16>(math::NkMin<uint32>(durationMs, 0xFFFF)) : 0xFFFF;
        eff.replay.delay = 0;

        if (ioctl(dev.ffFd, EVIOCSFF, &eff) >= 0) {
            dev.ffEffectId = eff.id;
            struct input_event play{};
            play.type = EV_FF;
            play.code = static_cast<uint16>(dev.ffEffectId);
            play.value = 1;
            write(dev.ffFd, &play, sizeof(play));
        }
    }

    void SetLEDColor(uint32 /*idx*/, uint32 /*rgba*/) override {
        // Non supportÃƒÆ’Ã‚Â©
    }

    bool HasMotion(uint32 /*idx*/) const noexcept override {
        return false;
    }

    const char* GetName() const noexcept override {
        return "evdev";
    }

private:
    NkArray<NkGamepadSnapshot, NK_MAX_GAMEPADS> mSnapshots{};
    NkArray<NkGamepadInfo, NK_MAX_GAMEPADS> mInfos{};
    NkArray<NkJsDevice, NK_MAX_GAMEPADS> mDevices{};
    int mInotifyFd = -1;

    // -----------------------------------------------------------------------
    // Scan /dev/input/jsN
    // -----------------------------------------------------------------------

    void ScanDevices() {
        DIR *dir = opendir("/dev/input");
        if (!dir)
            return;
        struct dirent *ent;
        while ((ent = readdir(dir)) != nullptr) {
            if (strncmp(ent->d_name, "js", 2) != 0)
                continue;
            char path[64];
            snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);
            TryOpenDevice(path);
        }
        closedir(dir);
    }

    void TryOpenDevice(const char *path) {
        // Trouver un slot libre
        uint32 slot = NK_MAX_GAMEPADS;
        for (uint32 i = 0; i < NK_MAX_GAMEPADS; ++i) {
            if (!mDevices[i].open) {
                slot = i;
                break;
            }
        }
        if (slot == NK_MAX_GAMEPADS)
            return;

        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0)
            return;

        NkJsDevice &dev = mDevices[slot];
        dev.fd = fd;
        dev.index = slot;
        dev.open = true;
        strncpy(dev.path, path, sizeof(dev.path) - 1);

        // Remplis les infos
        NkGamepadInfo &info = mInfos[slot];
        info.index = slot;
        info.type = NkGamepadType::NK_GP_TYPE_GENERIC;

        char name[128] = "Unknown Joystick";
        ioctl(fd, JSIOCGNAME(sizeof(name)), name);
        strncpy(info.name, name, sizeof(info.name) - 1);
        snprintf(info.id, sizeof(info.id), "%s", path);

        uint8 numAxes = 0, numBtns = 0;
        ioctl(fd, JSIOCGAXES, &numAxes);
        ioctl(fd, JSIOCGBUTTONS, &numBtns);
        info.numAxes = numAxes;
        info.numButtons = numBtns;

        // Chercher le event* correspondant (pour rumble)
        TryOpenForceFeedback(slot, path);

        mSnapshots[slot].connected = true;
        mSnapshots[slot].info = info;

        if (mInfos[slot].hasRumble)
            info.hasRumble = true;
    }

    void TryOpenForceFeedback(uint32 slot, const char *jsPath) {
        // Exemple : /dev/input/js0 ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ /dev/input/event3
        // On recherche dans /sys/class/input/jsN/device/event*/
        char sysPath[128];
        const char *jsName = strrchr(jsPath, '/');
        if (!jsName)
            return;
        ++jsName; // "js0"

        snprintf(sysPath, sizeof(sysPath), "/sys/class/input/%s/device", jsName);
        DIR *dir = opendir(sysPath);
        if (!dir)
            return;
        struct dirent *ent;
        while ((ent = readdir(dir)) != nullptr) {
            if (strncmp(ent->d_name, "event", 5) != 0)
                continue;
            char evPath[64];
            snprintf(evPath, sizeof(evPath), "/dev/input/%s", ent->d_name);
            int fd = open(evPath, O_RDWR | O_NONBLOCK);
            if (fd < 0)
                continue;

            // VÃƒÆ’Ã‚Â©rifier FF_RUMBLE supportÃƒÆ’Ã‚Â©
            unsigned char ffBits[FF_CNT / 8 + 1] = {};
            if (ioctl(fd, EVIOCGBIT(EV_FF, sizeof(ffBits)), ffBits) >= 0) {
                bool hasRumble = (ffBits[FF_RUMBLE / 8] >> (FF_RUMBLE % 8)) & 1;
                if (hasRumble) {
                    mDevices[slot].ffFd = fd;
                    strncpy(mDevices[slot].eventPath, evPath, sizeof(mDevices[slot].eventPath) - 1);
                    mInfos[slot].hasRumble = true;
                    break;
                }
            }
            close(fd);
        }
        closedir(dir);
    }

    void CloseDevice(NkJsDevice &dev) {
        if (!dev.open)
            return;
        if (dev.ffFd >= 0) {
            if (dev.ffEffectId >= 0)
                ioctl(dev.ffFd, EVIOCRMFF, dev.ffEffectId);
            close(dev.ffFd);
            dev.ffFd = -1;
        }
        if (dev.fd >= 0) {
            close(dev.fd);
            dev.fd = -1;
        }
        uint32 i = dev.index;
        dev = {};
        mSnapshots[i] = {};
        mInfos[i] = {};
    }

    // -----------------------------------------------------------------------
    // Hot-plug (inotify)
    // -----------------------------------------------------------------------

    void CheckHotPlug() {
        char buf[sizeof(struct inotify_event) + NAME_MAX + 1];
        ssize_t n;
        while ((n = read(mInotifyFd, buf, sizeof(buf))) > 0) {
            struct inotify_event *ev = reinterpret_cast<struct inotify_event *>(buf);
            if (ev->len > 0 && strncmp(ev->name, "js", 2) == 0) {
                char path[64];
                snprintf(path, sizeof(path), "/dev/input/%s", ev->name);
                if (ev->mask & (IN_CREATE | IN_ATTRIB))
                    TryOpenDevice(path);
                else if (ev->mask & IN_DELETE) {
                    for (auto &d : mDevices)
                        if (d.open && strcmp(d.path, path) == 0)
                            CloseDevice(d);
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // Mapping gÃƒÆ’Ã‚Â©nÃƒÆ’Ã‚Â©rique js ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ NkGamepadButton
    // -----------------------------------------------------------------------

    static NkGamepadButton JsButtonToNk(uint8 n) {
        using B = NkGamepadButton;
        // Standard linux mapping (Xbox-like)
        switch (n) {
            case 0:
                return B::NK_GP_SOUTH;
            case 1:
                return B::NK_GP_EAST;
            case 2:
                return B::NK_GP_WEST;
            case 3:
                return B::NK_GP_NORTH;
            case 4:
                return B::NK_GP_LB;
            case 5:
                return B::NK_GP_RB;
            case 6:
                return B::NK_GP_BACK;
            case 7:
                return B::NK_GP_START;
            case 8:
                return B::NK_GP_GUIDE;
            case 9:
                return B::NK_GP_LSTICK;
            case 10:
                return B::NK_GP_RSTICK;
            case 11:
                return B::NK_GP_DPAD_UP;
            case 12:
                return B::NK_GP_DPAD_DOWN;
            case 13:
                return B::NK_GP_DPAD_LEFT;
            case 14:
                return B::NK_GP_DPAD_RIGHT;
            default:
                return B::NK_GP_UNKNOWN;
        }
    }

    static NkGamepadAxis JsAxisToNk(uint8 n) {
        using A = NkGamepadAxis;
        switch (n) {
            case 0:
                return A::NK_GP_AXIS_LX;
            case 1:
                return A::NK_GP_AXIS_LY;
            case 2:
                return A::NK_GP_AXIS_LT;
            case 3:
                return A::NK_GP_AXIS_RX;
            case 4:
                return A::NK_GP_AXIS_RY;
            case 5:
                return A::NK_GP_AXIS_RT;
            case 6:
                return A::NK_GP_AXIS_DPAD_X;
            case 7:
                return A::NK_GP_AXIS_DPAD_Y;
            default:
                return A::NK_GAMEPAD_AXIS_MAX; // invalide
        }
    }
};

} // namespace nkentseu
