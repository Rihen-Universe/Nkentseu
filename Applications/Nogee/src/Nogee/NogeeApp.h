#pragma once
// =============================================================================
// Nogee/NogeeApp.h — application éditeur (hérite de NkApplication d'Engine/Noge)
// =============================================================================

#include "Noge/Core/NkApplication.h"
#include "UkConfig.h"
#include "Layers/EditorLayer.h"
#include "Layers/ViewportLayer.h"
#include "Layers/UILayer.h"
#include "Editor/NkEditorCamera.h"
#include "NKECS/World/NkWorld.h"
#include "Noge/ECS/Scene/NkSceneGraph.h"

namespace nkentseu {
	namespace noge {

		class NogeApp : public NkApplication {
			public:
				explicit NogeApp(const NogeAppConfig &config);
				~NogeApp() override;

			protected:
				void OnInit() override;
				void OnStart() override;
				void OnUpdate(float dt) override;
				void OnRender() override;
				void OnUIRender() override;
				void OnShutdown() override;
				void OnClose() override;
				void OnResize(nk_uint32 w, nk_uint32 h) override;

			private:
				NogeAppConfig mUkConfig;

				// Layers — pointeurs non-owning (ownership dans LayerStack)
				EditorLayer *mEditorLayer = nullptr;
				ViewportLayer *mViewportLayer = nullptr;
				UILayer *mUILayer = nullptr;

				// Caméra éditeur (owned ici, partagée avec ViewportLayer)
				NkEditorCamera *mEditorCamera = nullptr;

				// ── Monde ECS (2026-08-17) ────────────────────────────────
				// AVANT cette date, Nogee n'avait AUCUN monde : `SetWorld` et
				// `SetScene` etaient declares 3 fois et appeles 0 fois dans
				// tout le depot, donc `UILayer::mWorld` restait nul et
				// `RenderSceneTree`/`RenderInspector` sortaient immediatement
				// (`UILayer.cpp:475` et `:483`). Les panneaux d'arbre et
				// d'inspecteur n'avaient donc JAMAIS rien dessine.
				// Cf. ROADMAP Noge §10quater.
				//
				// Possede PAR VALEUR : `NkWorld` est un type valeur (cf.
				// `NkAgentEcsDemo/src/main.cpp:72`), et NogeApp survit a toutes
				// les couches qui le referencent.
				ecs::NkWorld mWorld;

				// Le graphe de scene tient une REFERENCE au monde et n'est ni
				// copiable ni assignable. Cree en OnInit, detruit en
				// OnShutdown — meme regime que la camera.
				ecs::NkSceneGraph *mScene = nullptr;
		};

	} // namespace noge
} // namespace nkentseu
