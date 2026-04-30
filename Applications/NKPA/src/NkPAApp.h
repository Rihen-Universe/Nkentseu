#pragma once
/**
 * @File    NkPAApp.h
 * @Brief   Classe principale de l'application NKPA.
 *
 *  Gestion :
 *    - Fenêtre NkWindow
 *    - Device NKRHI (API sélectionnable : software, OpenGL, Vulkan, DX11, DX12)
 *    - Boucle de jeu (BeginFrame / Render / EndFrame)
 *    - 12 créatures (4 marines, 4 terrestres, 4 mixtes)
 *    - Environnement 2 zones
 *    - UI overlay (panneau NKPA)
 */

#include "NKWindow/Core/NkWindow.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRHI/Core/NkGraphicsApi.h"
#include "NKTime/NkChrono.h"

#include "Renderer/NkPAMesh.h"
#include "Environment/NkPAEnvironment.h"
#include "UI/NkPAUI.h"

// ── Créatures marines ─────────────────────────────────────────────────────────
#include "Creatures/NkPAFish.h"
#include "Creatures/NkPAShark.h"
#include "Creatures/NkPAEel.h"
#include "Creatures/NkPAJellyfish.h"

// ── Créatures terrestres ──────────────────────────────────────────────────────
#include "Creatures/NkPASnake.h"
#include "Creatures/NkPACaterpillar.h"
#include "Creatures/NkPACentipede.h"
#include "Creatures/NkPAWorm.h"

// ── Créatures mixtes / volantes ───────────────────────────────────────────────
#include "Creatures/NkPALizard.h"
#include "Creatures/NkPATurtle.h"
#include "Creatures/NkPACat.h"
#include "Creatures/NkPABird.h"

namespace nkentseu {
namespace nkpa {

class NkPAApp {
public:
    NkPAApp()  = default;
    ~NkPAApp() = default;

    bool Init(int32 width = 1280, int32 height = 720,
              NkGraphicsApi api = NkGraphicsApi::NK_GFX_API_SOFTWARE);
    void Run();
    void Shutdown();

private:
    void Update(float32 dt);
    void Render();

    // ── Fenêtre & RHI ─────────────────────────────────────────────────────────
    NkWindow          mWindow;
    NkIDevice*        mDevice  = nullptr;
    NkICommandBuffer* mCmd     = nullptr;
    NkPipelineHandle  mPipeline;
    NkShaderHandle    mShader;

    // ── État ─────────────────────────────────────────────────────────────────
    bool           mRunning = false;
    int32          mWidth   = 1280;
    int32          mHeight  = 720;
    NkGraphicsApi  mApi     = NkGraphicsApi::NK_GFX_API_SOFTWARE;
    NkChrono       mChrono;
    NkPAUIState    mUIState;

    // ── UI ───────────────────────────────────────────────────────────────────
    NkPAUI      mUI;

    // ── Environnement ────────────────────────────────────────────────────────
    Environment mEnv;

    // ── Créatures marines (zone eau) ─────────────────────────────────────────
    static constexpr int32 NUM_FISH      = 3;
    static constexpr int32 NUM_SHARK     = 1;
    static constexpr int32 NUM_EEL       = 2;
    static constexpr int32 NUM_JELLYFISH = 2;

    Fish       mFish[NUM_FISH];
    Shark      mSharks[NUM_SHARK];
    Eel        mEels[NUM_EEL];
    Jellyfish  mJellyfish[NUM_JELLYFISH];

    // ── Créatures terrestres (zone sol) ──────────────────────────────────────
    static constexpr int32 NUM_SNAKE      = 1;
    static constexpr int32 NUM_CATERPILLAR= 2;
    static constexpr int32 NUM_CENTIPEDE  = 1;
    static constexpr int32 NUM_WORM       = 2;

    Snake        mSnakes[NUM_SNAKE];
    Caterpillar  mCaterpillars[NUM_CATERPILLAR];
    Centipede    mCentipedes[NUM_CENTIPEDE];
    Worm         mWorms[NUM_WORM];

    // ── Créatures mixtes / volantes ───────────────────────────────────────────
    static constexpr int32 NUM_LIZARD  = 1;
    static constexpr int32 NUM_TURTLE  = 1;
    static constexpr int32 NUM_CAT     = 1;
    static constexpr int32 NUM_BIRD    = 2;

    Lizard   mLizards[NUM_LIZARD];
    Turtle   mTurtles[NUM_TURTLE];
    Cat      mCats[NUM_CAT];
    Bird     mBirds[NUM_BIRD];

    // ── Mesh ─────────────────────────────────────────────────────────────────
    MeshBuilder mMesh;
};

} // namespace nkpa
} // namespace nkentseu
