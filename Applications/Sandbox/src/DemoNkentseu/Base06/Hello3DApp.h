// =============================================================================
// Examples/Hello3D/Hello3DApp.h
// =============================================================================
// Exemple complet de l'utilisation du moteur Nkentseu — affiche un cube PBR
// en rotation avec une lumière directionnelle et une caméra orbitale.
//
// DÉMONTRE :
//   • NkApplication + NkEngineLayer
//   • NkGameObjectFactory::Create()
//   • NkComponentHandle (AddComponent, GetComponent)
//   • NkSceneGraph + NkSceneScript
//   • NkRenderSystem (mesh + matériau + lumière + caméra)
//   • NkTransformSystem (hiérarchie parent/enfant)
//   • NkBehaviour (script de rotation)
// =============================================================================
#pragma once

#include "Nkentseu/Nkentseu.h"

using namespace nkentseu;
using namespace nkentseu::ecs;

// =============================================================================
// RotateBehaviour — fait tourner l'entité sur l'axe Y
// =============================================================================
class RotateBehaviour : public NkBehaviour {
    public:
        float32 speed = 45.f;  ///< Degrés par seconde

        void OnUpdate(float dt) override {
            auto* go = GetGameObject();
            if (!go) return;
            go->Transform()->Rotate(
                NkQuatf::FromAxisAngle({0, 1, 0}, speed * dt * (3.14159f / 180.f)));
        }

        const char* GetTypeName() const override { return "RotateBehaviour"; }
};

// =============================================================================
// OrbitCameraBehaviour — caméra orbitale autour de l'origine
// =============================================================================
class OrbitCameraBehaviour : public NkBehaviour {
    public:
        float32 radius = 5.f;
        float32 angle  = 0.f;
        float32 speed  = 30.f;  ///< Degrés par seconde

        void OnUpdate(float dt) override {
            auto* go = GetGameObject();
            if (!go) return;
            angle += speed * dt;
            if (angle >= 360.f) angle -= 360.f;
            const float32 rad = angle * (3.14159f / 180.f);
            const float32 x   = radius * NkCosf(rad);
            const float32 z   = radius * NkSinf(rad);
            go->SetPosition(x, 2.f, z);
            // La caméra regarde vers l'origine
            go->Transform()->SetLocalRotation(
                NkQuatf::LookAt({x, 2.f, z}, {0, 0, 0}, {0, 1, 0}));
        }

        const char* GetTypeName() const override { return "OrbitCameraBehaviour"; }
};

// =============================================================================
// MainScene — script de la scène principale
// =============================================================================
class MainScene : public NkSceneScript {
    public:
        NkGameObject mCube;
        NkGameObject mCamera;
        NkGameObject mSun;

        void OnBeginPlay() override {
            SetupCamera();
            SetupLighting();
            SetupCube();
        }

        void OnTick(float dt) override { (void)dt; }

    private:
        void SetupCamera() {
            mCamera = NkGameObjectFactory::Create<NkGameObject>(*mWorld, "Camera");

            // Composant caméra
            auto cam = mCamera.AddComponent<NkCameraComponent>();
            cam->projection = NkCameraProjection::Perspective;
            cam->fovDeg     = 60.f;
            cam->nearClip   = 0.1f;
            cam->farClip    = 1000.f;
            cam->aspect     = 16.f / 9.f;
            cam->priority   = 10;

            // Position initiale
            mCamera.SetPosition(0.f, 2.f, 5.f);

            // Script d'orbite
            mCamera.AddBehaviour<OrbitCameraBehaviour>();
        }

        void SetupLighting() {
            // Lumière directionnelle principale (soleil)
            mSun = NkGameObjectFactory::Create<NkGameObject>(*mWorld, "Sun");
            auto light = mSun.AddComponent<NkLightComponent>();
            light->type       = NkLightType::Directional;
            light->color      = { 1.f, 0.95f, 0.9f, 1.f };
            light->intensity  = 2.f;
            light->castShadow = true;

            // Direction : 45° vers le bas, 45° sur Y
            mSun.Transform()->SetLocalRotationEuler(45.f, 45.f, 0.f);

            // Lumière ambiante (point light faible pour fill)
            auto fill = NkGameObjectFactory::Create<NkGameObject>(*mWorld, "FillLight");
            auto fl   = fill.AddComponent<NkLightComponent>();
            fl->type      = NkLightType::Ambient;
            fl->intensity = 0.3f;
            fl->color     = { 0.4f, 0.5f, 0.7f, 1.f };
        }

        void SetupCube() {
            // Cube principal avec PBR
            mCube = NkGameObjectFactory::Create<NkGameObject>(*mWorld, "Cube");
            mCube.SetPosition(0.f, 0.f, 0.f);

            // Mesh
            auto mesh = mCube.AddComponent<NkMeshComponent>();
            mesh->meshPath   = "Assets/Meshes/cube.nkmesh";
            mesh->castShadow = true;

            // Matériau PBR
            auto mat = mCube.AddComponent<NkMaterialComponent>();
            mat->SetMaterial(0, "Assets/Materials/pbr_metallic.nkmat");

            // Script de rotation
            auto* rotate = mCube.AddBehaviour<RotateBehaviour>();
            rotate->speed = 45.f;

            // Enfant : cube plus petit en orbite autour du premier
            auto child = NkGameObjectFactory::Create<NkGameObject>(*mWorld, "SmallCube");
            child.SetPosition(2.f, 0.f, 0.f);
            child.SetScale({0.3f, 0.3f, 0.3f});

            auto childMesh = child.AddComponent<NkMeshComponent>();
            childMesh->meshPath = "Assets/Meshes/cube.nkmesh";

            auto childMat = child.AddComponent<NkMaterialComponent>();
            childMat->SetMaterial(0, "Assets/Materials/pbr_emissive.nkmat");

            child.SetParent(mCube);  // Suit la rotation du parent

            auto* childRotate = child.AddBehaviour<RotateBehaviour>();
            childRotate->speed = -90.f;  // Rotation inverse
        }
};

// =============================================================================
// Hello3DApp — Application principale
// =============================================================================
class Hello3DApp : public NkApplication {
    public:
        explicit Hello3DApp(const NkApplicationConfig& cfg)
            : NkApplication(cfg) {}

        void OnInit() override {
            // Pousse la couche moteur en premier
            PushLayer(new NkEngineLayer());
        }

        void OnStart() override {
            auto& eng = NkEngineLayer::Get();

            // Enregistre la scène principale
            eng.RegisterScene("Main", [](NkWorld& w) -> NkSceneGraph* {
                auto* scene = new NkSceneGraph(w, "Main");
                scene->SetScript<MainScene>();
                return scene;
            });

            // Charge la scène
            eng.LoadScene("Main");
        }

        void OnResize(nk_uint32 w, nk_uint32 h) override {
            if (NkEngineLayer::IsReady()) {
                NkEngineLayer::Get().Resize(w, h);
            }
        }
};

// =============================================================================
// Point d'entrée
// =============================================================================
NkApplication* NkMainApplication(const NkApplicationConfig& cfg) {
    NkApplicationConfig c = cfg;
    c.appName             = "Hello 3D — Nkentseu";
    c.appVersion          = "1.0.0";
    c.windowConfig.title  = "Hello 3D";
    c.windowConfig.width  = 1280;
    c.windowConfig.height = 720;
    c.fixedTimeStep       = 1.f / 60.f;
    c.debugMode           = true;
    return new Hello3DApp(c);
}