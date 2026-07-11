/**
 * @File    NkPAApp.cpp
 * @Brief   Implémentation de la boucle principale NKPA.
 */

#include "NkPAApp.h"

#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKWindow/Core/NkWESystem.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"

#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Core/NkGraphicsApi.h"

// Shaders : compilation / conversion NkSL (SPIR-V Vulkan, bridge software)
#include "NKSL/ShaderConvert/NkShaderConvert.h"
#include "NKRHI/SL/NkSLIntegration.h"
#include "NKRHI/SL/NkSWShaderBridge.h"

#include "NKLogger/NkLog.h"

#include <cstdlib>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::nkpa;

// ─────────────────────────────────────────────────────────────────────────────
//  Shaders NKPA — passthrough « couleur par sommet », décliné par backend.
//  Sommet : vec3 position (déjà en NDC après MeshBuilder::BuildNDC) + vec4 RGBA.
//  Un NkShaderDesc vide ne fournit AUCUN stage → pipeline invalide (crash GPU /
//  rendu sans couleur en software). On charge donc de vrais shaders ici.
//
//  ⚠️ Vulkan : la compilation GLSL→SPIR-V au runtime (glslang) corrompt le heap
//  sous clang-mingw (bug moteur reproduit aussi par la démo Model). On embarque
//  donc du SPIR-V PRÉ-COMPILÉ (glslangValidator -V) → aucun appel glslang au run.
//  Sources d'origine (GLSL 460) :
//    vert : layout(location=0) in vec3 aPosition; layout(location=1) in vec4 aColor;
//           layout(location=0) out vec4 vColor;
//           void main() { gl_Position = vec4(aPosition,1.0); vColor = aColor; }
//    frag : layout(location=0) in vec4 vColor; layout(location=0) out vec4 outColor;
//           void main() { outColor = vColor; }
// ─────────────────────────────────────────────────────────────────────────────

static const uint32 kNkpaVertSpv[244] = {
	0x07230203, 0x00010000, 0x0008000b, 0x0000001f, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
	0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
	0x0009000f, 0x00000000, 0x00000004, 0x6e69616d, 0x00000000, 0x0000000d, 0x00000012, 0x0000001b,
	0x0000001d, 0x00030003, 0x00000002, 0x000001cc, 0x00040005, 0x00000004, 0x6e69616d, 0x00000000,
	0x00060005, 0x0000000b, 0x505f6c67, 0x65567265, 0x78657472, 0x00000000, 0x00060006, 0x0000000b,
	0x00000000, 0x505f6c67, 0x7469736f, 0x006e6f69, 0x00070006, 0x0000000b, 0x00000001, 0x505f6c67,
	0x746e696f, 0x657a6953, 0x00000000, 0x00070006, 0x0000000b, 0x00000002, 0x435f6c67, 0x4470696c,
	0x61747369, 0x0065636e, 0x00070006, 0x0000000b, 0x00000003, 0x435f6c67, 0x446c6c75, 0x61747369,
	0x0065636e, 0x00030005, 0x0000000d, 0x00000000, 0x00050005, 0x00000012, 0x736f5061, 0x6f697469,
	0x0000006e, 0x00040005, 0x0000001b, 0x6c6f4376, 0x0000726f, 0x00040005, 0x0000001d, 0x6c6f4361,
	0x0000726f, 0x00030047, 0x0000000b, 0x00000002, 0x00050048, 0x0000000b, 0x00000000, 0x0000000b,
	0x00000000, 0x00050048, 0x0000000b, 0x00000001, 0x0000000b, 0x00000001, 0x00050048, 0x0000000b,
	0x00000002, 0x0000000b, 0x00000003, 0x00050048, 0x0000000b, 0x00000003, 0x0000000b, 0x00000004,
	0x00040047, 0x00000012, 0x0000001e, 0x00000000, 0x00040047, 0x0000001b, 0x0000001e, 0x00000000,
	0x00040047, 0x0000001d, 0x0000001e, 0x00000001, 0x00020013, 0x00000002, 0x00030021, 0x00000003,
	0x00000002, 0x00030016, 0x00000006, 0x00000020, 0x00040017, 0x00000007, 0x00000006, 0x00000004,
	0x00040015, 0x00000008, 0x00000020, 0x00000000, 0x0004002b, 0x00000008, 0x00000009, 0x00000001,
	0x0004001c, 0x0000000a, 0x00000006, 0x00000009, 0x0006001e, 0x0000000b, 0x00000007, 0x00000006,
	0x0000000a, 0x0000000a, 0x00040020, 0x0000000c, 0x00000003, 0x0000000b, 0x0004003b, 0x0000000c,
	0x0000000d, 0x00000003, 0x00040015, 0x0000000e, 0x00000020, 0x00000001, 0x0004002b, 0x0000000e,
	0x0000000f, 0x00000000, 0x00040017, 0x00000010, 0x00000006, 0x00000003, 0x00040020, 0x00000011,
	0x00000001, 0x00000010, 0x0004003b, 0x00000011, 0x00000012, 0x00000001, 0x0004002b, 0x00000006,
	0x00000014, 0x3f800000, 0x00040020, 0x00000019, 0x00000003, 0x00000007, 0x0004003b, 0x00000019,
	0x0000001b, 0x00000003, 0x00040020, 0x0000001c, 0x00000001, 0x00000007, 0x0004003b, 0x0000001c,
	0x0000001d, 0x00000001, 0x00050036, 0x00000002, 0x00000004, 0x00000000, 0x00000003, 0x000200f8,
	0x00000005, 0x0004003d, 0x00000010, 0x00000013, 0x00000012, 0x00050051, 0x00000006, 0x00000015,
	0x00000013, 0x00000000, 0x00050051, 0x00000006, 0x00000016, 0x00000013, 0x00000001, 0x00050051,
	0x00000006, 0x00000017, 0x00000013, 0x00000002, 0x00070050, 0x00000007, 0x00000018, 0x00000015,
	0x00000016, 0x00000017, 0x00000014, 0x00050041, 0x00000019, 0x0000001a, 0x0000000d, 0x0000000f,
	0x0003003e, 0x0000001a, 0x00000018, 0x0004003d, 0x00000007, 0x0000001e, 0x0000001d, 0x0003003e,
	0x0000001b, 0x0000001e, 0x000100fd, 0x00010038,
};
static const uint32 kNkpaFragSpv[94] = {
	0x07230203, 0x00010000, 0x0008000b, 0x0000000d, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
	0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
	0x0007000f, 0x00000004, 0x00000004, 0x6e69616d, 0x00000000, 0x00000009, 0x0000000b, 0x00030010,
	0x00000004, 0x00000007, 0x00030003, 0x00000002, 0x000001cc, 0x00040005, 0x00000004, 0x6e69616d,
	0x00000000, 0x00050005, 0x00000009, 0x4374756f, 0x726f6c6f, 0x00000000, 0x00040005, 0x0000000b,
	0x6c6f4376, 0x0000726f, 0x00040047, 0x00000009, 0x0000001e, 0x00000000, 0x00040047, 0x0000000b,
	0x0000001e, 0x00000000, 0x00020013, 0x00000002, 0x00030021, 0x00000003, 0x00000002, 0x00030016,
	0x00000006, 0x00000020, 0x00040017, 0x00000007, 0x00000006, 0x00000004, 0x00040020, 0x00000008,
	0x00000003, 0x00000007, 0x0004003b, 0x00000008, 0x00000009, 0x00000003, 0x00040020, 0x0000000a,
	0x00000001, 0x00000007, 0x0004003b, 0x0000000a, 0x0000000b, 0x00000001, 0x00050036, 0x00000002,
	0x00000004, 0x00000000, 0x00000003, 0x000200f8, 0x00000005, 0x0004003d, 0x00000007, 0x0000000c,
	0x0000000b, 0x0003003e, 0x00000009, 0x0000000c, 0x000100fd, 0x00010038,
};

static NkShaderHandle LoadNkpaShaders(NkIDevice *device, NkGraphicsApi api) {
	NkShaderDesc desc;
	desc.debugName = "NkpaShader";

	// Les sources GLSL/HLSL sont des littéraux statiques → durée de vie infinie
	// (AddGLSL/AddHLSL stockent des pointeurs RAW). Vulkan = SPIR-V embarqué.

	switch (api) {
		// ── OpenGL : GLSL 450, entrée "main" ────────────────────────────────────
		case NkGraphicsApi::NK_GFX_API_OPENGL: {
			static const char *kVert = "#version 450 core\n"
									   "layout(location=0) in vec3 aPosition;\n"
									   "layout(location=1) in vec4 aColor;\n"
									   "layout(location=0) out vec4 vColor;\n"
									   "void main() { gl_Position = vec4(aPosition, 1.0); vColor = aColor; }\n";
			static const char *kFrag = "#version 450 core\n"
									   "layout(location=0) in vec4 vColor;\n"
									   "layout(location=0) out vec4 outColor;\n"
									   "void main() { outColor = vColor; }\n";
			desc.AddGLSL(NkShaderStage::NK_VERTEX, kVert, "main");
			desc.AddGLSL(NkShaderStage::NK_FRAGMENT, kFrag, "main");
			break;
		}

		// ── Vulkan : SPIR-V pré-compilé embarqué (pas de glslang au runtime) ────
		case NkGraphicsApi::NK_GFX_API_VULKAN: {
			desc.AddSPIRV(NkShaderStage::NK_VERTEX, kNkpaVertSpv, sizeof(kNkpaVertSpv));
			desc.AddSPIRV(NkShaderStage::NK_FRAGMENT, kNkpaFragSpv, sizeof(kNkpaFragSpv));
			break;
		}

		// ── DirectX 11 / 12 : HLSL, entrées VSMain / PSMain ─────────────────────
		case NkGraphicsApi::NK_GFX_API_DX11:
		case NkGraphicsApi::NK_GFX_API_DX12: {
			static const char *kVert =
				"struct VSInput { float3 pos : POSITION; float4 col : COLOR; };\n"
				"struct VSOutput { float4 pos : SV_POSITION; float4 col : COLOR; };\n"
				"VSOutput VSMain(VSInput input) { VSOutput o; o.pos = float4(input.pos, 1.0); o.col = input.col; "
				"return o; }\n";
			static const char *kFrag = "struct PSInput { float4 pos : SV_POSITION; float4 col : COLOR; };\n"
									   "float4 PSMain(PSInput input) : SV_Target { return input.col; }\n";
			desc.AddHLSL(NkShaderStage::NK_VERTEX, kVert, "VSMain");
			desc.AddHLSL(NkShaderStage::NK_FRAGMENT, kFrag, "PSMain");
			break;
		}

		// ── Software : NkSL compilé en lambdas C++ via le bridge ────────────────
		case NkGraphicsApi::NK_GFX_API_SOFTWARE: {
			const char *kVert = "@location(0) in vec3 aPosition;\n"
								"@location(1) in vec4 aColor;\n"
								"@location(0) out vec4 vColor;\n"
								"@stage(vertex)\n@entry\n"
								"void vert_main() { vColor = aColor; gl_Position = vec4(aPosition, 1.0); }\n";
			const char *kFrag = "@location(0) in vec4 vColor;\n"
								"@location(0) out vec4 FragColor;\n"
								"@stage(fragment)\n@entry\n"
								"void frag_main() { FragColor = vColor; }\n";
			swbridge::NkSWBridgeResult c =
				swbridge::NkCompileSources(NkString(kVert), NkString(kFrag), "nkpa.vert.sw.sksl", "nkpa.frag.sw.sksl");
			if (!c.success) {
				logger_src.Error("[NKPA] Software NkSL: {0}", c.error.CStr());
				return {};
			}
			// Heap-alloc : NkSoftwareDevice::CreateShader prend possession et delete.
			auto *vsFn = new NkVertexShaderSoftware(traits::NkMove(c.vertFn));
			auto *psFn = new NkPixelShaderSoftware(traits::NkMove(c.fragFn));
			desc.AddSWFn(NkShaderStage::NK_VERTEX, static_cast<const void *>(vsFn));
			desc.AddSWFn(NkShaderStage::NK_FRAGMENT, static_cast<const void *>(psFn));
			break;
		}

		default:
			logger_src.Error("[NKPA] API shader non supportée: {0}", NkGraphicsApiName(api));
			return {};
	}

	NkShaderHandle sh = device->CreateShader(desc);
	if (!sh.IsValid())
		logger_src.Error("[NKPA] CreateShader a échoué");
	return sh;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Init
// ─────────────────────────────────────────────────────────────────────────────

bool NkPAApp::Init(int32 width, int32 height, NkGraphicsApi api) {
	mWidth = width;
	mHeight = height;
	mApi = api;

	// ── Compilateur NkSL (cache SPIR-V Vulkan + bridge software) ───────────────
	nksl::InitCompiler("./shader_cache");

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

	// ── Device RHI ────────────────────────────────────────────────────────────
	NkDeviceInitInfo init{};
	init.api = mApi;
	init.surface = mWindow.GetSurfaceDesc();
	init.width = (uint32)mWidth;
	init.height = (uint32)mHeight;
	if (mApi == NkGraphicsApi::NK_GFX_API_SOFTWARE) {
		init.context.software.threading = true;
		init.context.software.useSSE = true;
	}

	logger_src.Info("[NKPA] Backend: {0}", NkGraphicsApiName(mApi));
	mDevice = NkDeviceFactory::Create(init);
	if (!mDevice || !mDevice->IsValid()) {
		logger_src.Error("[NKPA] Echec création device {0}", NkGraphicsApiName(mApi));
		mWindow.Close();
		return false;
	}

	// ── Shader (couleur par sommet, par backend) ──────────────────────────────
	mShader = LoadNkpaShaders(mDevice, mApi);
	if (!mShader.IsValid()) {
		logger_src.Error("[NKPA] Echec création shader");
		Shutdown();
		return false;
	}

	// ── Pipeline (triangle list, couleur par sommet) ──────────────────────────
	NkVertexLayout layout;
	layout.AddAttribute(0, 0, NkGPUFormat::NK_RGB32_FLOAT, 0, "POSITION", 0)
		.AddAttribute(1, 0, NkGPUFormat::NK_RGBA32_FLOAT, 3 * sizeof(float32), "COLOR", 0)
		.AddBinding(0, sizeof(PaVertex));

	NkGraphicsPipelineDesc pd{};
	pd.shader = mShader;
	pd.vertexLayout = layout;
	pd.topology = NkPrimitiveTopology::NK_TRIANGLE_LIST;
	pd.rasterizer = NkRasterizerDesc::Default();
	pd.depthStencil = NkDepthStencilDesc::NoDepth();
	pd.blend = NkBlendDesc::Alpha();
	pd.renderPass = mDevice->GetSwapchainRenderPass();

	mPipeline = mDevice->CreateGraphicsPipeline(pd);
	if (!mPipeline.IsValid()) {
		logger_src.Error("[NKPA] Echec création pipeline");
		Shutdown();
		return false;
	}

	// ── Command buffer ────────────────────────────────────────────────────────
	mCmd = mDevice->CreateCommandBuffer(NkCommandBufferType::NK_GRAPHICS);
	if (!mCmd || !mCmd->IsValid()) {
		logger_src.Error("[NKPA] Echec création command buffer");
		Shutdown();
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
		mWidth = e->GetWidth();
		mHeight = e->GetHeight();
	});

	// ── NKUI ─────────────────────────────────────────────────────────────────
	mUI.Init(mWidth, mHeight);

	// Callbacks souris → alimentation NKUI (dans NkPAApp.cpp car inclut NKWindow)
	ev.AddEventCallback<NkMouseMoveEvent>(
		[&](NkMouseMoveEvent *e) { mUI.SetMousePos((float32)e->GetX(), (float32)e->GetY()); });
	ev.AddEventCallback<NkMouseButtonPressEvent>([&](NkMouseButtonPressEvent *e) {
		int32 btn = (e->GetButton() == NkMouseButton::NK_MB_LEFT)	  ? 0
					: (e->GetButton() == NkMouseButton::NK_MB_RIGHT)  ? 1
					: (e->GetButton() == NkMouseButton::NK_MB_MIDDLE) ? 2
																	  : -1;
		if (btn >= 0)
			mUI.SetMouseButton(btn, true);
	});
	ev.AddEventCallback<NkMouseButtonReleaseEvent>([&](NkMouseButtonReleaseEvent *e) {
		int32 btn = (e->GetButton() == NkMouseButton::NK_MB_LEFT)	  ? 0
					: (e->GetButton() == NkMouseButton::NK_MB_RIGHT)  ? 1
					: (e->GetButton() == NkMouseButton::NK_MB_MIDDLE) ? 2
																	  : -1;
		if (btn >= 0)
			mUI.SetMouseButton(btn, false);
	});
	ev.AddEventCallback<NkMouseWheelVerticalEvent>(
		[&](NkMouseWheelVerticalEvent *e) { mUI.AddMouseWheel((float32)e->GetDeltaY()); });

	// ── Environnement ─────────────────────────────────────────────────────────
	mEnv.Init((float32)mWidth, (float32)mHeight);
	float32 wY = mEnv.WaterY();
	float32 wH = (float32)mHeight;
	float32 wW = (float32)mWidth;

	// ── Créatures ─────────────────────────────────────────────────────────────
	srand(12345u);

	// Marines — zone eau [wY+20 .. wH-20]
	auto RandW = [&]() -> float32 { return 80.f + (float32)rand() / (float32)RAND_MAX * (wW - 160.f); };
	auto RandWY = [&]() -> float32 { return wY + 30.f + (float32)rand() / (float32)RAND_MAX * (wH - wY - 50.f); };

	for (int32 i = 0; i < NUM_FISH; ++i)
		mFish[i].Init({RandW(), RandWY()}, mWidth, mHeight);
	for (int32 i = 0; i < NUM_SHARK; ++i)
		mSharks[i].Init({RandW(), RandWY()}, mWidth, mHeight);
	for (int32 i = 0; i < NUM_EEL; ++i)
		mEels[i].Init({RandW(), RandWY()}, mWidth, mHeight);
	for (int32 i = 0; i < NUM_JELLYFISH; ++i)
		mJellyfish[i].Init({RandW(), RandWY()}, mWidth, mHeight);

	// Terrestres — zone terre [wY+15 .. wY+80]
	auto RandLY = [&]() -> float32 { return wY + 20.f + (float32)rand() / (float32)RAND_MAX * 60.f; };

	for (int32 i = 0; i < NUM_SNAKE; ++i)
		mSnakes[i].Init({RandW(), RandLY()}, mWidth, mHeight);
	for (int32 i = 0; i < NUM_CATERPILLAR; ++i)
		mCaterpillars[i].Init({RandW(), RandLY()}, mWidth, mHeight);
	for (int32 i = 0; i < NUM_CENTIPEDE; ++i)
		mCentipedes[i].Init({RandW(), RandLY()}, mWidth, mHeight);
	for (int32 i = 0; i < NUM_WORM; ++i)
		mWorms[i].Init({RandW(), RandLY()}, mWidth, mHeight);

	// Mixtes — toute la scène
	for (int32 i = 0; i < NUM_LIZARD; ++i)
		mLizards[i].Init({RandW(), RandLY()}, mWidth, mHeight);
	for (int32 i = 0; i < NUM_TURTLE; ++i)
		mTurtles[i].Init({RandW(), RandWY()}, mWidth, mHeight);
	for (int32 i = 0; i < NUM_CAT; ++i)
		mCats[i].Init({RandW(), RandLY()}, mWidth, mHeight);
	for (int32 i = 0; i < NUM_BIRD; ++i) {
		// Oiseaux dans le ciel
		float32 birdY = 40.f + (float32)rand() / (float32)RAND_MAX * (wY * 0.5f);
		mBirds[i].Init({RandW(), birdY}, mWidth, mHeight);
	}

	mRunning = true;
	mChrono = NkChrono{};
	logger_src.Info("[NKPA] Démarrage — 12 espèces, 20 individus");
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Run
// ─────────────────────────────────────────────────────────────────────────────

void NkPAApp::Run() {
	NkEventSystem &ev = NkEvents();

	while (mRunning) {
		mUI.BeginInput(); // reset transients avant les événements
		ev.PollEvents();
		if (!mRunning)
			break;

		uint32 sw = mDevice->GetSwapchainWidth();
		uint32 sh = mDevice->GetSwapchainHeight();

		if (sw == 0 || sh == 0)
			continue;
		if (sw != (uint32)mWidth || sh != (uint32)mHeight) {
			mWidth = (int32)sw;
			mHeight = (int32)sh;
			mDevice->OnResize(sw, sh);
			mEnv.Init((float32)mWidth, (float32)mHeight);
		}

		NkElapsedTime elapsed = mChrono.Reset();
		float32 dt = (float32)elapsed.seconds;
		if (dt > 0.05f)
			dt = 0.05f;
		if (dt < 0.0001f)
			dt = 0.016f;

		// NKUI input + widgets
		mUI.BeginInput();
		mUI.BuildFrame(dt, mUIState);

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

	for (int32 i = 0; i < NUM_FISH; ++i)
		mFish[i].Update(dt);
	for (int32 i = 0; i < NUM_SHARK; ++i)
		mSharks[i].Update(dt);
	for (int32 i = 0; i < NUM_EEL; ++i)
		mEels[i].Update(dt);
	for (int32 i = 0; i < NUM_JELLYFISH; ++i)
		mJellyfish[i].Update(dt);

	for (int32 i = 0; i < NUM_SNAKE; ++i)
		mSnakes[i].Update(dt);
	for (int32 i = 0; i < NUM_CATERPILLAR; ++i)
		mCaterpillars[i].Update(dt);
	for (int32 i = 0; i < NUM_CENTIPEDE; ++i)
		mCentipedes[i].Update(dt);
	for (int32 i = 0; i < NUM_WORM; ++i)
		mWorms[i].Update(dt);

	for (int32 i = 0; i < NUM_LIZARD; ++i)
		mLizards[i].Update(dt);
	for (int32 i = 0; i < NUM_TURTLE; ++i)
		mTurtles[i].Update(dt);
	for (int32 i = 0; i < NUM_CAT; ++i)
		mCats[i].Update(dt);
	for (int32 i = 0; i < NUM_BIRD; ++i)
		mBirds[i].Update(dt);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Render
// ─────────────────────────────────────────────────────────────────────────────

void NkPAApp::Render() {
	NkFrameContext frame{};
	if (!mDevice->BeginFrame(frame))
		return;

	mMesh.Clear();

	// 1. Environnement (fond)
	mEnv.Draw(mMesh);

	// 2. Créatures marines (dans l'eau)
	for (int32 i = 0; i < NUM_JELLYFISH; ++i)
		mJellyfish[i].Draw(mMesh);
	for (int32 i = 0; i < NUM_EEL; ++i)
		mEels[i].Draw(mMesh);
	for (int32 i = 0; i < NUM_FISH; ++i)
		mFish[i].Draw(mMesh);
	for (int32 i = 0; i < NUM_SHARK; ++i)
		mSharks[i].Draw(mMesh);
	for (int32 i = 0; i < NUM_TURTLE; ++i)
		mTurtles[i].Draw(mMesh);

	// 3. Créatures terrestres
	for (int32 i = 0; i < NUM_WORM; ++i)
		mWorms[i].Draw(mMesh);
	for (int32 i = 0; i < NUM_CATERPILLAR; ++i)
		mCaterpillars[i].Draw(mMesh);
	for (int32 i = 0; i < NUM_CENTIPEDE; ++i)
		mCentipedes[i].Draw(mMesh);
	for (int32 i = 0; i < NUM_SNAKE; ++i)
		mSnakes[i].Draw(mMesh);
	for (int32 i = 0; i < NUM_LIZARD; ++i)
		mLizards[i].Draw(mMesh);
	for (int32 i = 0; i < NUM_CAT; ++i)
		mCats[i].Draw(mMesh);

	// 4. Oiseaux (au-dessus de tout)
	for (int32 i = 0; i < NUM_BIRD; ++i)
		mBirds[i].Draw(mMesh);

	// 5. UI overlay NKUI
	mUI.RenderToMesh(mMesh);

	if (mMesh.IsEmpty()) {
		mDevice->SubmitAndPresent(mCmd);
		mDevice->EndFrame(frame);
		return;
	}

	// Conversion pixels → NDC
	NkVector<PaVertex> ndc = mMesh.BuildNDC((float32)mWidth, (float32)mHeight);

	NkBufferHandle vbuf =
		mDevice->CreateBuffer(NkBufferDesc::Vertex((uint64)ndc.Size() * sizeof(PaVertex), ndc.Begin()));

	mCmd->Begin();
	NkRect2D area{0, 0, mWidth, mHeight};
	if (mCmd->BeginRenderPass(mDevice->GetSwapchainRenderPass(), mDevice->GetSwapchainFramebuffer(), area)) {
		NkViewport vp;
		vp.x = 0.f;
		vp.y = 0.f;
		vp.width = (float32)mWidth;
		vp.height = (float32)mHeight;
		vp.minDepth = 0.f;
		vp.maxDepth = 1.f;
		mCmd->SetViewport(vp);
		mCmd->SetScissor(area);
		mCmd->BindGraphicsPipeline(mPipeline);
		mCmd->BindVertexBuffer(0, vbuf);
		mCmd->Draw((uint32)ndc.Size());
		mCmd->EndRenderPass();
	}
	mCmd->End();

	mDevice->SubmitAndPresent(mCmd);
	mDevice->EndFrame(frame);

	if (vbuf.IsValid())
		mDevice->DestroyBuffer(vbuf);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Shutdown
// ─────────────────────────────────────────────────────────────────────────────

void NkPAApp::Shutdown() {
	if (mCmd) {
		mDevice->DestroyCommandBuffer(mCmd);
		mCmd = nullptr;
	}
	if (mPipeline.IsValid()) {
		mDevice->DestroyPipeline(mPipeline);
	}
	if (mShader.IsValid()) {
		mDevice->DestroyShader(mShader);
	}
	if (mDevice) {
		NkDeviceFactory::Destroy(mDevice);
		mDevice = nullptr;
	}
	mWindow.Close();

	nksl::ShutdownCompiler();
}
