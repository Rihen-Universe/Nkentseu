/**
 * @File    NkPAApp.cpp
 * @Brief   Implémentation de la boucle principale NKPA.
 */

#include "NkPAApp.h"
#include "NkPAConfig.h"

#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKWindow/Core/NkWESystem.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"

// Rendu NKCanvas (pile Pong/NKCode) — device+swapchain+renderer 2D, 5 backends.
#include "NKCanvas/Core/NkContextDesc.h"
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKCanvas/Renderer/Core/NkIRenderer2D.h"
#include "NKCanvas/Renderer/Core/NkRenderer2DTypes.h" // NkVertex2D, NkView2D
#include "NKMath/NkRectangle.h"						  // NkRect2i

#include "NKLogger/NkLog.h"

#include <cstdlib>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::nkpa;

// ─────────────────────────────────────────────────────────────────────────────
//  Init
// ─────────────────────────────────────────────────────────────────────────────

bool NkPAApp::Init(int32 width, int32 height, NkGraphicsApi api) {
	mWidth = width;
	mHeight = height;
	mApi = api;

	// ── Fenêtre ───────────────────────────────────────────────────────────────
	NkWindowConfig winCfg;
	winCfg.title = "NKPA — Procedural Animation";
	winCfg.width = mWidth;
	winCfg.height = mHeight;
	winCfg.centered = true;
	winCfg.resizable = true;
	if (!mWindow.Create(winCfg)) {
		logger_src.Error("[NKPA] Echec création fenêtre");
		return false;
	}

	// ── Cible NKCanvas (device + swapchain + renderer 2D) — 5 backends ─────────
	NkContextDesc desc;
	desc.api = mApi;
	logger_src.Info("[NKPA] Backend: {0}", NkGraphicsApiName(mApi));
	mTarget = new renderer::NkRenderWindow(mWindow, desc);
	if (!mTarget || !mTarget->IsValid()) {
		logger_src.Error("[NKPA] Echec NkRenderWindow ({0})", NkGraphicsApiName(mApi));
		return false;
	}

	// ── Événements fenêtre ───────────────────────────────────────────────────
	NkEventSystem &ev = NkEvents();
	ev.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent *) { mRunning = false; });
	ev.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent *e) {
		if (e->GetKey() == NkKey::NK_ESCAPE)
			mRunning = false;
		if (e->GetKey() == NkKey::NK_SPACE)
			mUIState.paused = !mUIState.paused;
		if (e->GetKey() == NkKey::NK_F1)
			mUIState.showUI = !mUIState.showUI;
	});
	ev.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent *e) {
		// Source AUTORITATIVE de la taille. On mémorise la demande ; elle est
		// appliquée à un point sûr de la boucle (après PollEvents, avant Render).
		const int32 w = (int32)e->GetWidth();
		const int32 h = (int32)e->GetHeight();
		if (w > 0 && h > 0) {
			mPendingW = w;
			mPendingH = h;
			mResizePending = true;
		}
	});

	// ── UI NKGui via NKCanvas (atlas de police) ────────────────────────────────
	if (!mGui.Init(mTarget->GetRenderer(), mWidth, mHeight))
		logger_src.Warn("[NKPA] UI NKGui indisponible → simulation sans UI");

	// Callbacks souris → alimentation NKGui (dans NkPAApp.cpp car inclut NKWindow)
	ev.AddEventCallback<NkMouseMoveEvent>(
		[&](NkMouseMoveEvent *e) { mGui.SetMousePos((float32)e->GetX(), (float32)e->GetY()); });
	ev.AddEventCallback<NkMouseButtonPressEvent>([&](NkMouseButtonPressEvent *e) {
		int32 btn = (e->GetButton() == NkMouseButton::NK_MB_LEFT)	  ? 0
					: (e->GetButton() == NkMouseButton::NK_MB_RIGHT)  ? 1
					: (e->GetButton() == NkMouseButton::NK_MB_MIDDLE) ? 2
																	  : -1;
		if (btn >= 0)
			mGui.SetMouseButton(btn, true);
	});
	ev.AddEventCallback<NkMouseButtonReleaseEvent>([&](NkMouseButtonReleaseEvent *e) {
		int32 btn = (e->GetButton() == NkMouseButton::NK_MB_LEFT)	  ? 0
					: (e->GetButton() == NkMouseButton::NK_MB_RIGHT)  ? 1
					: (e->GetButton() == NkMouseButton::NK_MB_MIDDLE) ? 2
																	  : -1;
		if (btn >= 0)
			mGui.SetMouseButton(btn, false);
	});
	ev.AddEventCallback<NkMouseWheelVerticalEvent>(
		[&](NkMouseWheelVerticalEvent *e) { mGui.AddMouseWheel((float32)e->GetDeltaY()); });

	// ── Monde (environnement + créatures) dimensionné à la VUE centrale ────────
	// La sim est rendue dans le viewport central de l'éditeur ; on l'ensemence
	// donc aux dimensions de cette vue (mise à jour si les panneaux/taille changent).
	{
		int32 vx, vy, vw, vh;
		mGui.GetViewportRect(mUIState, vx, vy, vw, vh);
		mViewW = vw;
		mViewH = vh;
		SeedWorld(vw, vh);
	}

	mRunning = true;
	mChrono = NkChrono{};
	logger_src.Info("[NKPA] Démarrage — 12 espèces, 20 individus");
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  SeedWorld — (re)crée l'environnement + place les créatures dans un monde
//  de dimensions (worldW × worldH) = la VUE centrale de l'éditeur.
// ─────────────────────────────────────────────────────────────────────────────

void NkPAApp::SeedWorld(int32 worldW, int32 worldH) {
	mEnv.Init((float32)worldW, (float32)worldH);
	const float32 wY = mEnv.WaterY();
	const float32 wH = (float32)worldH;
	const float32 wW = (float32)worldW;

	srand(12345u);
	auto RandW = [&]() -> float32 { return 80.f + (float32)rand() / (float32)RAND_MAX * (wW - 160.f); };
	auto RandWY = [&]() -> float32 { return wY + 30.f + (float32)rand() / (float32)RAND_MAX * (wH - wY - 50.f); };
	auto RandLY = [&]() -> float32 { return wY + 20.f + (float32)rand() / (float32)RAND_MAX * 60.f; };

	// Marines — zone eau
	for (int32 i = 0; i < NUM_FISH; ++i)
		mFish[i].Init({RandW(), RandWY()}, worldW, worldH);
	for (int32 i = 0; i < NUM_SHARK; ++i)
		mSharks[i].Init({RandW(), RandWY()}, worldW, worldH);
	for (int32 i = 0; i < NUM_EEL; ++i)
		mEels[i].Init({RandW(), RandWY()}, worldW, worldH);
	for (int32 i = 0; i < NUM_JELLYFISH; ++i)
		mJellyfish[i].Init({RandW(), RandWY()}, worldW, worldH);

	// Terrestres — zone sol
	for (int32 i = 0; i < NUM_SNAKE; ++i)
		mSnakes[i].Init({RandW(), RandLY()}, worldW, worldH);
	for (int32 i = 0; i < NUM_CATERPILLAR; ++i)
		mCaterpillars[i].Init({RandW(), RandLY()}, worldW, worldH);
	for (int32 i = 0; i < NUM_CENTIPEDE; ++i)
		mCentipedes[i].Init({RandW(), RandLY()}, worldW, worldH);
	for (int32 i = 0; i < NUM_WORM; ++i)
		mWorms[i].Init({RandW(), RandLY()}, worldW, worldH);

	// Mixtes
	for (int32 i = 0; i < NUM_LIZARD; ++i)
		mLizards[i].Init({RandW(), RandLY()}, worldW, worldH);
	for (int32 i = 0; i < NUM_TURTLE; ++i)
		mTurtles[i].Init({RandW(), RandWY()}, worldW, worldH);
	for (int32 i = 0; i < NUM_CAT; ++i)
		mCats[i].Init({RandW(), RandLY()}, worldW, worldH);
	for (int32 i = 0; i < NUM_BIRD; ++i) {
		const float32 birdY = 40.f + (float32)rand() / (float32)RAND_MAX * (wY * 0.5f);
		mBirds[i].Init({RandW(), birdY}, worldW, worldH);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Run
// ─────────────────────────────────────────────────────────────────────────────

void NkPAApp::Run() {
	NkEventSystem &ev = NkEvents();

	while (mRunning) {
		mGui.BeginInput(); // reset transients avant les événements
		ev.PollEvents();
		if (!mRunning)
			break;

		// Resize FENÊTRE (demandé par l'événement, appliqué ICI = point sûr) →
		// recréer la swapchain + prévenir l'UI. La taille de la vue centrale est
		// recalculée juste après (GetViewportRect) → la render-texture suit.
		if (mResizePending && (mPendingW != mWidth || mPendingH != mHeight)) {
			mWidth = mPendingW;
			mHeight = mPendingH;
			mTarget->OnResize((uint32)mWidth, (uint32)mHeight);
			mGui.OnResize(mWidth, mHeight);
		}
		mResizePending = false;
		if (mWidth <= 0 || mHeight <= 0)
			continue;

		NkElapsedTime elapsed = mChrono.Reset();
		float32 dt = (float32)elapsed.seconds;
		if (dt > 0.05f)
			dt = 0.05f;
		if (dt < 0.0001f)
			dt = 0.016f;

		// UI : construire les widgets de la frame (peut basculer les panneaux).
		mGui.BuildFrame(dt, mUIState, NkPAConfig::ApiName(mApi));

		// Changement de backend demandé depuis l'UI → sauver la config + redémarrer.
		if (mUIState.requestRestart && mUIState.requestBackendIndex >= 0) {
			ApplyBackendRequest();
			break;
		}

		// VUE CENTRALE : si sa TAILLE change (resize fenêtre / panneau basculé),
		// re-semer le monde à ces dimensions (le rendu place la sim dans ce rect).
		{
			int32 vx, vy, vw, vh;
			mGui.GetViewportRect(mUIState, vx, vy, vw, vh);
			if (vw != mViewW || vh != mViewH) {
				mViewW = vw;
				mViewH = vh;
				SeedWorld(vw, vh);
			}
		}

		if (!mUIState.paused)
			Update(dt * mUIState.speedScale);

		Render();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Update
// ─────────────────────────────────────────────────────────────────────────────

void NkPAApp::Update(float32 dt) {
	mEnv.Update(dt);

	// Une espèce désactivée dans l'UI est gelée (ni Update, ni Draw).
	const bool *en = mUIState.speciesEnabled;

	if (en[(int32)Species::Fish])
		for (int32 i = 0; i < NUM_FISH; ++i)
			mFish[i].Update(dt);
	if (en[(int32)Species::Shark])
		for (int32 i = 0; i < NUM_SHARK; ++i)
			mSharks[i].Update(dt);
	if (en[(int32)Species::Eel])
		for (int32 i = 0; i < NUM_EEL; ++i)
			mEels[i].Update(dt);
	if (en[(int32)Species::Jellyfish])
		for (int32 i = 0; i < NUM_JELLYFISH; ++i)
			mJellyfish[i].Update(dt);

	if (en[(int32)Species::Snake])
		for (int32 i = 0; i < NUM_SNAKE; ++i)
			mSnakes[i].Update(dt);
	if (en[(int32)Species::Caterpillar])
		for (int32 i = 0; i < NUM_CATERPILLAR; ++i)
			mCaterpillars[i].Update(dt);
	if (en[(int32)Species::Centipede])
		for (int32 i = 0; i < NUM_CENTIPEDE; ++i)
			mCentipedes[i].Update(dt);
	if (en[(int32)Species::Worm])
		for (int32 i = 0; i < NUM_WORM; ++i)
			mWorms[i].Update(dt);

	if (en[(int32)Species::Lizard])
		for (int32 i = 0; i < NUM_LIZARD; ++i)
			mLizards[i].Update(dt);
	if (en[(int32)Species::Turtle])
		for (int32 i = 0; i < NUM_TURTLE; ++i)
			mTurtles[i].Update(dt);
	if (en[(int32)Species::Cat])
		for (int32 i = 0; i < NUM_CAT; ++i)
			mCats[i].Update(dt);
	if (en[(int32)Species::Bird])
		for (int32 i = 0; i < NUM_BIRD; ++i)
			mBirds[i].Update(dt);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Render
// ─────────────────────────────────────────────────────────────────────────────

void NkPAApp::Render() {
	if (!mTarget || !mTarget->IsValid())
		return;

	mMesh.Clear();

	// 1. Environnement (fond)
	mEnv.Draw(mMesh);

	const bool *en = mUIState.speciesEnabled;

	// 2. Créatures marines (dans l'eau)
	if (en[(int32)Species::Jellyfish])
		for (int32 i = 0; i < NUM_JELLYFISH; ++i)
			mJellyfish[i].Draw(mMesh);
	if (en[(int32)Species::Eel])
		for (int32 i = 0; i < NUM_EEL; ++i)
			mEels[i].Draw(mMesh);
	if (en[(int32)Species::Fish])
		for (int32 i = 0; i < NUM_FISH; ++i)
			mFish[i].Draw(mMesh);
	if (en[(int32)Species::Shark])
		for (int32 i = 0; i < NUM_SHARK; ++i)
			mSharks[i].Draw(mMesh);
	if (en[(int32)Species::Turtle])
		for (int32 i = 0; i < NUM_TURTLE; ++i)
			mTurtles[i].Draw(mMesh);

	// 3. Créatures terrestres
	if (en[(int32)Species::Worm])
		for (int32 i = 0; i < NUM_WORM; ++i)
			mWorms[i].Draw(mMesh);
	if (en[(int32)Species::Caterpillar])
		for (int32 i = 0; i < NUM_CATERPILLAR; ++i)
			mCaterpillars[i].Draw(mMesh);
	if (en[(int32)Species::Centipede])
		for (int32 i = 0; i < NUM_CENTIPEDE; ++i)
			mCentipedes[i].Draw(mMesh);
	if (en[(int32)Species::Snake])
		for (int32 i = 0; i < NUM_SNAKE; ++i)
			mSnakes[i].Draw(mMesh);
	if (en[(int32)Species::Lizard])
		for (int32 i = 0; i < NUM_LIZARD; ++i)
			mLizards[i].Draw(mMesh);
	if (en[(int32)Species::Cat])
		for (int32 i = 0; i < NUM_CAT; ++i)
			mCats[i].Draw(mMesh);

	// 4. Oiseaux (au-dessus de tout)
	if (en[(int32)Species::Bird])
		for (int32 i = 0; i < NUM_BIRD; ++i)
			mBirds[i].Draw(mMesh);

	// ── Conversion du mesh (pixels-monde) → NkVertex2D pour NKCanvas ───────────
	const NkVector<PaVertex> &px = mMesh.RawPixels();
	const uint32 nVerts = (uint32)px.Size();
	mSimVerts.Resize(nVerts);
	mSimIdx.Resize(nVerts); // triangle-list → indices séquentiels (0,1,2,3,…)
	for (uint32 i = 0; i < nVerts; ++i) {
		const PaVertex &v = px[i];
		renderer::NkVertex2D &d = mSimVerts[i];
		d.x = v.x;
		d.y = v.y;
		d.u = 0.f;
		d.v = 0.f;
		d.r = (uint8)(v.r * 255.f);
		d.g = (uint8)(v.g * 255.f);
		d.b = (uint8)(v.b * 255.f);
		d.a = (uint8)(v.a * 255.f);
		mSimIdx[i] = i;
	}

	// Rect central (le panneau viewport) en pixels fenêtre.
	int32 vx, vy, vw, vh;
	mGui.GetViewportRect(mUIState, vx, vy, vw, vh);

	renderer::NkIRenderer2D *r = mTarget->GetRenderer();
	if (!r)
		return;

	// ── (Re)création de la render-texture si le panneau central a changé de taille.
	bool rtJustCreated = false;
	if (!mRT.IsValid() || mRTW != vw || mRTH != vh) {
		mRT.Destroy();
		if (mRT.Create(*r, (uint32)vw, (uint32)vh)) {
			mRTW = vw;
			mRTH = vh;
			mRTTexture.SetGPUId(mRT.GetColorTextureGPUId());
			rtJustCreated = true;
		} else {
			mRTW = mRTH = 0;
		}
	}

	mTarget->Clear(); // ouvre la frame + clear le back buffer

	// ── 1) PASSE OFFSCREEN : la SIM est rendue DANS la render-texture ──────────
	// (nested : au milieu de la frame ; NkRenderTexture flush+bind l'offscreen,
	//  on dessine, puis flush+unbind → retour back buffer).
	// On SAUTE la passe offscreen la frame EXACTE où la RT vient d'être (re)créée
	// (typiquement un redimensionnement) : la swapchain/le depth viennent d'être
	// recréés → basculer de render pass CETTE frame-là est fragile (Vulkan). Le
	// panneau reste noir une frame, puis reprend. Effet invisible en pratique.
	const bool haveRT = mRT.IsValid() && !mSimVerts.Empty() && !rtJustCreated;
	if (haveRT) {
		mRT.Clear(renderer::NkColor2D{0, 0, 0, 255}); // l'environnement (ciel/eau) couvre tout
		// La sim est en coords monde 0..vw × 0..vh = vue par défaut de la RT.
		r->DrawVertices(mSimVerts.Data(), (uint32)mSimVerts.Size(), mSimIdx.Data(),
						(uint32)mSimIdx.Size(), nullptr);
		mRT.Display();
	}

	// ── 2) BACK BUFFER : vue pixel 1:1, on colle la RT dans le panneau central,
	//        puis l'UI par-dessus. Vue pixel-parfaite depuis la taille COURANTE
	//        (pas GetDefaultView, sinon UI « zoomée » + souris décalée au resize).
	r->SetViewport(math::NkRect2i{0, 0, mWidth, mHeight});
	renderer::NkView2D uiView;
	uiView.center = {(float32)mWidth * 0.5f, (float32)mHeight * 0.5f};
	uiView.size = {(float32)mWidth, (float32)mHeight};
	r->SetView(uiView);

	if (haveRT) {
		// UV : flip V pour OpenGL (texture FBO bottom-up) ; direct pour DX/VK/SW.
		const bool flipV = (mApi == NkGraphicsApi::NK_GFX_API_OPENGL);
		math::NkRect2f uv = flipV ? math::NkRect2f{0.f, 1.f, 1.f, -1.f} : math::NkRect2f{0.f, 0.f, 1.f, 1.f};
		r->DrawTexturedRect(math::NkRect2f{(float32)vx, (float32)vy, (float32)vw, (float32)vh}, &mRTTexture,
							renderer::NkColor2D::White, uv);
	}

	mGui.Render((uint32)mWidth, (uint32)mHeight);

	mTarget->Display();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Shutdown
// ─────────────────────────────────────────────────────────────────────────────

void NkPAApp::ApplyBackendRequest() {
	static const NkGraphicsApi kApis[5] = {NkGraphicsApi::NK_GFX_API_OPENGL, NkGraphicsApi::NK_GFX_API_VULKAN,
										   NkGraphicsApi::NK_GFX_API_DX11, NkGraphicsApi::NK_GFX_API_DX12,
										   NkGraphicsApi::NK_GFX_API_SOFTWARE};
	const int32 idx = mUIState.requestBackendIndex;
	if (idx < 0 || idx >= 5)
		return;
	NkPAConfig cfg;
	cfg.backend = kApis[idx];
	const NkString path = mConfigPath.Empty() ? NkString(NkPAConfig::DefaultPath()) : mConfigPath;
	cfg.Save(path);
	logger_src.Info("[NKPA] Backend → {0} (config sauvee) : redemarrage", NkPAConfig::ApiName(cfg.backend));
	mRestartRequested = true;
	mRunning = false;
}

void NkPAApp::Shutdown() {
	mRT.Destroy();
	mGui.Shutdown();
	if (mTarget) {
		delete mTarget; // NkRenderWindow détruit device + swapchain + renderer
		mTarget = nullptr;
	}
	mWindow.Close();
}
