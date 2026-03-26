#pragma once
/**
 * @File    NkPAUI.h
 * @Brief   Pont NKUI ↔ NKPA — interface sans inclure NkUI.h (pimpl).
 *
 *  NkPAUI isole les headers NKUI via le pimpl idiom pour éviter le conflit
 *  entre nkentseu::NkKey (NKWindow, uint32) et nkentseu::NkKey (NkUI, uint8).
 *
 *  L'application alimente l'input via SetMousePos/SetMouseButton/AddMouseWheel
 *  (appeles depuis NkPAApp.cpp qui inclut les headers NKWindow).
 *  NKUI lui-meme n'est compile que dans NkPAUI.cpp.
 */

#include "NKCore/NkTypes.h"
#include "Renderer/NkPAMesh.h"

namespace nkentseu {
namespace nkpa {

// ─── État exposé à l'application ──────────────────────────────────────────────

struct NkPAUIState {
    bool  paused     = false;
    bool  showUI     = true;
    float speedScale = 1.f;   ///< Multiplicateur global de vitesse [0.1 .. 3.0]
    int32 fishCount  = 3;     ///< [1..6]
    int32 sharkCount = 1;     ///< [0..3]
};

// ─── Classe principale ────────────────────────────────────────────────────────

class NkPAUI {
public:
    NkPAUI();
    ~NkPAUI();

    NkPAUI(const NkPAUI&)            = delete;
    NkPAUI& operator=(const NkPAUI&) = delete;

    void Init(int32 w, int32 h);
    void OnResize(int32 w, int32 h);

    // ── Input (appelé depuis NkPAApp.cpp avec les événements NKWindow) ────────
    /// Réinitialise les transients de la frame (à appeler avant PollEvents).
    void BeginInput();
    void SetMousePos(float32 x, float32 y);
    void SetMouseButton(int32 btn, bool down);
    void AddMouseWheel(float32 dy);

    /// Construit les widgets NKUI pour la frame courante.
    /// @param dt    Delta-time de la frame
    /// @param state État lu/écrit par les widgets
    void BuildFrame(float32 dt, NkPAUIState& state);

    /// Convertit les DrawLists NKUI en PaVertex et les ajoute à mb.
    void RenderToMesh(MeshBuilder& mb) const;

private:
    struct Impl;   // défini dans NkPAUI.cpp — include NkUI.h en privé
    Impl* mImpl = nullptr;
};

} // namespace nkpa
} // namespace nkentseu
