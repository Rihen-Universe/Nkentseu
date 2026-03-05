# ⚙️ Functions

> 23 éléments

[🏠 Accueil](../index.md) | [🎯 Types](./index.md)

## Liste

- **[`ClipPoint`](../files/NkSafeArea.h.md#clippoint)** — Clipe un point dans la zone sûre (returns false si hors zone)
- **[`ColorPicker`](../files/NkDialogs.h.md#colorpicker)** — Ouvre un sélecteur de couleur.
- **[`GetConnectedCount`](../files/NkGamepadSystem.h.md#getconnectedcount)** — Nombre de manettes actuellement connectées.
- **[`GetInfo`](../files/NkGamepadSystem.h.md#getinfo)** — Infos sur la manette à l'indice idx (0-based).
- **[`GetState`](../files/NkGamepadSystem.h.md#getstate)** — Snapshot complet de l'état courant.
- **[`IsPress`](../files/NkKeyboardEvents.h.md#ispress)** — < true = touche maintenue, l'OS génère des répétitions.
- **[`NkClose`](../files/NkSystem.h.md#nkclose)** — Libère toutes les ressources du framework. Appeler avant la fin de nkmain() / main().
- **[`NkDisableDropTarget`](../files/NkDropSystem.h.md#nkdisabledroptarget)** — Désactive le support drag&drop.
- **[`NkEnableDropTarget`](../files/NkDropSystem.h.md#nkenabledroptarget)** — Active le support drag&drop sur la fenêtre donnée (handle natif).
- **[`NkGetEventImpl`](../files/NkSystem.h.md#nkgeteventimpl)** — Accède à l'implémentation d'événements active (plateforme courante).
- **[`NkInitialise`](../files/NkSystem.h.md#nkinitialise)** — Initialise le framework NkWindow. Doit être appelé une fois en début de programme (ou dans nkmain()). *
- **[`NkScancodeFromDOMCode`](../files/NkScancode.h.md#nkscancodefromdomcode)** — Convertit un DOM KeyboardEvent.code (WASM) vers NkScancode USB HID
- **[`NkScancodeFromLinux`](../files/NkScancode.h.md#nkscancodefromlinux)** — Convertit un keycode Linux evdev (XCB/XLib) vers NkScancode USB HID
- **[`NkScancodeFromMac`](../files/NkScancode.h.md#nkscancodefrommac)** — Convertit un keyCode macOS (NSEvent.keyCode) vers NkScancode USB HID
- **[`NkScancodeFromWin32`](../files/NkScancode.h.md#nkscancodefromwin32)** — Convertit un scancode Win32 Set-1 (MAPVK_VK_TO_VSC) vers NkScancode USB HID
- **[`NkScancodeToKey`](../files/NkScancode.h.md#nkscancodetokey)** — Convertit NkScancode vers NkKey (position US-QWERTY invariante)
- **[`NkScancodeToString`](../files/NkScancode.h.md#nkscancodetostring)** — Nom lisible du scancode (ex: "SC_A", "SC_SPACE")
- **[`OpenFileDialog`](../files/NkDialogs.h.md#openfiledialog)** — Ouvre un dialogue de sélection de fichier.
- **[`Rumble`](../files/NkGamepadSystem.h.md#rumble)** — Lance une vibration. Implémentation peut ignorer si non supporté.
- **[`SaveFileDialog`](../files/NkDialogs.h.md#savefiledialog)** — Ouvre un dialogue de sauvegarde de fichier.
- **[`Shutdown`](../files/IEventImpl.h.md#shutdown)** — Appelé par IWindowImpl::Close() avant destruction du handle.
- **[`ToNkDuration`](../files/NkClock.h.md#tonkduration)** — Convert std::chrono duration to NkDuration.
- **[`UsableWidth`](../files/NkSafeArea.h.md#usablewidth)** — Surface utilisable (en pixels)
