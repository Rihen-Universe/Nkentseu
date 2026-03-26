#pragma once
// =============================================================================
// NkScene.h
// Graphe de scène du NKRenderer — entités, composants, hiérarchie.
//
// Architecture ECS légère :
//   NkEntity     → identifiant unique
//   NkComponent  → données attachées à une entité
//   NkScene      → conteneur + update + culling + rendu
//
// Composants disponibles :
//   NkTransformComponent     — position/rotation/scale en hiérarchie
//   NkMeshComponent          — mesh + matériaux
//   NkLightComponent         — lumière (directionnelle, point, spot, area)
//   NkCameraComponent        — caméra de rendu
//   NkParticleComponent      — système de particules
//   NkTrailComponent         — trail/ruban
//   NkText3DComponent        — texte 3D (billboard ou extrudé)
//   NkSkyComponent           — skybox / HDRI
//   NkReflectionProbeComponent — sonde de réflexion IBL locale
//   NkOccluderComponent      — occulteur pour culling
// =============================================================================
#include "NKRenderer/Core/NkRenderTypes.h"
#include "NKRenderer/Mesh/NkMesh.h"
#include "NKRenderer/Material/NkMaterial.h"
#include "NKRenderer/Core/NkTexture.h"
#include "NKRenderer/Renderer/NkRenderer3D.h"
#include "NKRenderer/Renderer/NkRenderer2D.h"
#include "NKRenderer/Text/NkTextRenderer.h"
#include "NKRenderer/VFX/NkVFXSystem.h"
#include "NKRHI/Core/NkIDevice.h"

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        // Identifiant d'entité
        // =============================================================================
        using NkEntityID = uint64;
        static constexpr NkEntityID NK_INVALID_ENTITY = 0;

        // =============================================================================
        // Composant de base
        // =============================================================================
        struct NkComponent {
            NkEntityID owner   = NK_INVALID_ENTITY;
            bool       enabled = true;
            virtual ~NkComponent() = default;
            virtual const char* TypeName() const = 0;
        };

        // =============================================================================
        // Transform (hiérarchique)
        // =============================================================================
        struct NkTransformComponent : NkComponent {
            NkVec3f    localPosition = {0,0,0};
            NkVec3f    localRotation = {0,0,0}; // Euler XYZ (degrés)
            NkVec3f    localScale    = {1,1,1};
            NkEntityID parent        = NK_INVALID_ENTITY;
            NkVector<NkEntityID> children;

            // Calculés et mis en cache
            mutable NkMat4f worldMatrix     = NkMat4f::Identity();
            mutable NkMat4f localMatrix     = NkMat4f::Identity();
            mutable bool    dirty           = true;

            const char* TypeName() const override { return "Transform"; }

            NkTransformComponent& SetPosition(const NkVec3f& p) { localPosition=p; dirty=true; return *this; }
            NkTransformComponent& SetRotation(const NkVec3f& r) { localRotation=r; dirty=true; return *this; }
            NkTransformComponent& SetScale   (const NkVec3f& s) { localScale=s;    dirty=true; return *this; }
            NkTransformComponent& SetPosition(float x,float y,float z) { return SetPosition({x,y,z}); }
            NkTransformComponent& SetScale   (float s)           { return SetScale({s,s,s}); }

            NkMat4f ComputeLocalMatrix() const;
            void    UpdateWorldMatrix(const NkMat4f& parentWorld = NkMat4f::Identity());
            NkVec3f GetWorldPosition() const { return {worldMatrix.data[12], worldMatrix.data[13], worldMatrix.data[14]}; }
            NkVec3f GetForward()       const { return {-worldMatrix.data[8], -worldMatrix.data[9], -worldMatrix.data[10]}; }
            NkVec3f GetRight()         const { return { worldMatrix.data[0],  worldMatrix.data[1],  worldMatrix.data[2]}; }
            NkVec3f GetUp()            const { return { worldMatrix.data[4],  worldMatrix.data[5],  worldMatrix.data[6]}; }

            void Translate(const NkVec3f& delta) { localPosition = localPosition + delta; dirty = true; }
            void Rotate   (const NkVec3f& delta) { localRotation = localRotation + delta; dirty = true; }
            void LookAt(const NkVec3f& target, const NkVec3f& up = {0,1,0});
        };

        // =============================================================================
        // Composant Mesh
        // =============================================================================
        struct NkMeshComponent : NkComponent {
            NkStaticMesh*   mesh            = nullptr;
            NkDynamicMesh*  dynamicMesh     = nullptr;
            NkSkinnedMesh*  skinnedMesh     = nullptr;
            bool            castShadow      = true;
            bool            receiveShadow   = true;
            bool            visible         = true;
            float           lodBias         = 0.f;
            // Surcharge de matériaux (null = utiliser ceux du mesh)
            NkVector<NkMaterial*> materialOverrides;
            // Outline (sélection)
            bool            drawOutline     = false;
            NkColor4f       outlineColor    = {1,0.5f,0,1};
            // Wireframe overlay
            bool            drawWireframe   = false;
            bool            drawVertices    = false;
            bool            drawNormals     = false;
            float           normalLength    = 0.05f;

            const char* TypeName() const override { return "Mesh"; }

            NkMaterial* GetMaterial(uint32 idx) const;
            void SetMaterialOverride(uint32 idx, NkMaterial* m);
        };

        // =============================================================================
        // Composant Lumière
        // =============================================================================
        struct NkLightComponent : NkComponent {
            NkLight  light;
            bool     visualize = false; // afficher le gizmo de lumière

            const char* TypeName() const override { return "Light"; }

            // Helpers
            NkLightComponent& SetColor    (const NkColor4f& c) { light.color=c; return *this; }
            NkLightComponent& SetIntensity(float v)             { light.intensity=v; return *this; }
            NkLightComponent& SetRange    (float v)             { light.range=v; return *this; }
            NkLightComponent& SetType     (NkLightType t)       { light.type=t; return *this; }
            NkLightComponent& SetConeAngles(float inner, float outer) {
                light.innerCone=inner; light.outerCone=outer; return *this;
            }
            NkLightComponent& CastShadow  (bool v)             { light.castShadow=v; return *this; }
        };

        // =============================================================================
        // Composant Caméra
        // =============================================================================
        struct NkCameraComponent : NkComponent {
            NkCamera camera;
            bool     isMainCamera   = false;
            uint32   priority       = 0;    // ordre de rendu (highest = dernier rendu)
            NkRenderTarget* target  = nullptr; // null = swapchain
            NkPostProcessSettings postProcess;
            NkRenderMode      renderMode = NkRenderMode::Solid;

            const char* TypeName() const override { return "Camera"; }

            // Met à jour la position/direction depuis le transform
            void SyncFromTransform(const NkTransformComponent& t);
        };

        // =============================================================================
        // Composant Particules
        // =============================================================================
        struct NkParticleComponent : NkComponent {
            NkParticleSystem* system = nullptr;
            bool autoDestroy = false;   // détruire l'entité quand les particules s'éteignent

            const char* TypeName() const override { return "Particle"; }

            void Play()    { if (system) system->Play(); }
            void Pause()   { if (system) system->Pause(); }
            void Stop()    { if (system) system->Stop(); }
            void Restart() { if (system) system->Restart(); }
        };

        // =============================================================================
        // Composant Trail
        // =============================================================================
        struct NkTrailComponent : NkComponent {
            NkTrail* trail = nullptr;

            const char* TypeName() const override { return "Trail"; }
        };

        // =============================================================================
        // Composant Texte 3D
        // =============================================================================
        enum class NkText3DMode { Billboard, Anchored, Extruded };

        struct NkText3DComponent : NkComponent {
            NkString     text;
            NkFont*      font         = nullptr;
            NkText3DMode mode         = NkText3DMode::Billboard;
            NkTextStyle  style;
            NkTextAlign  align        = NkTextAlign::Center;
            float        worldSize    = 0.5f;
            NkText3DParams extrudeParams;
            // Mesh généré (pour mode Extruded)
            NkStaticMesh* extrudedMesh = nullptr;
            NkMaterial*   extrudeMat   = nullptr;
            bool          dirty        = true; // reconstruire le mesh si texte changé

            const char* TypeName() const override { return "Text3D"; }
            NkText3DComponent& SetText(const char* t) { text=t; dirty=true; return *this; }
        };

        // =============================================================================
        // Composant Skybox
        // =============================================================================
        struct NkSkyComponent : NkComponent {
            NkSkySettings sky;
            const char* TypeName() const override { return "Sky"; }
        };

        // =============================================================================
        // Reflection Probe (IBL locale)
        // =============================================================================
        struct NkReflectionProbeComponent : NkComponent {
            NkVec3f        position   = {0,0,0};
            float          radius     = 10.f;
            NkTextureCube* envMap     = nullptr;
            NkTextureCube* irradiance = nullptr;
            NkTextureCube* prefilter  = nullptr;
            float          blendWeight= 1.f;
            bool           autoCapture= false;
            uint32         captureSize= 256;

            const char* TypeName() const override { return "ReflectionProbe"; }
        };

        // =============================================================================
        // Composant Occulteur (pour frustum/occlusion culling)
        // =============================================================================
        struct NkOccluderComponent : NkComponent {
            NkAABB  aabb;
            bool    isOccluder  = true;  // bloque la vue
            bool    isOccludee  = true;  // peut être masqué
            const char* TypeName() const override { return "Occluder"; }
        };

        // =============================================================================
        // Composant Script (comportement personnalisé)
        // =============================================================================
        struct NkScriptComponent : NkComponent {
            const char* TypeName() const override { return "Script"; }
            virtual void OnStart()            {}
            virtual void OnUpdate(float dt)   {}
            virtual void OnDestroy()          {}
            virtual void OnEnable()           {}
            virtual void OnDisable()          {}
            virtual void OnCollision(NkEntityID other) {}
        };

        // =============================================================================
        // NkEntity — entité de la scène
        // =============================================================================
        class NkScene;

        class NkEntity {
            public:
                NkEntity() = default;
                explicit NkEntity(NkEntityID id, NkScene* scene)
                    : mID(id), mScene(scene) {}

                NkEntityID GetID()    const { return mID; }
                bool       IsValid()  const { return mID != NK_INVALID_ENTITY; }
                NkScene*   GetScene() const { return mScene; }

                // Ajout / accès composants
                template<typename T, typename... Args>
                T* AddComponent(Args&&... args);

                template<typename T>
                T* GetComponent();

                template<typename T>
                const T* GetComponent() const;

                template<typename T>
                bool HasComponent() const;

                template<typename T>
                void RemoveComponent();

                // Raccourcis
                NkTransformComponent* GetTransform();
                NkMeshComponent*      GetMesh();
                NkLightComponent*     GetLight();
                NkCameraComponent*    GetCamera();

                // Hiérarchie
                void SetParent(NkEntity parent);
                NkEntity GetParent() const;
                NkEntity AddChild(const char* name = "Entity");
                NkVector<NkEntity> GetChildren() const;

                // Métadonnées
                void        SetName(const char* name);
                const char* GetName() const;
                void        SetTag(const char* tag);
                const char* GetTag() const;
                void        SetActive(bool active);
                bool        IsActive() const;
                void        Destroy();

                bool operator==(const NkEntity& o) const { return mID == o.mID; }
                bool operator!=(const NkEntity& o) const { return mID != o.mID; }

            private:
                NkEntityID mID    = NK_INVALID_ENTITY;
                NkScene*   mScene = nullptr;
        };

        // =============================================================================
        // NkScene — conteneur principal
        // =============================================================================
        class NkScene {
            public:
                NkScene()  = default;
                ~NkScene() { Destroy(); }
                NkScene(const NkScene&) = delete;
                NkScene& operator=(const NkScene&) = delete;

                // ── Cycle de vie ──────────────────────────────────────────────────────────
                bool Create(NkIDevice* device, const char* name = "Scene");
                void Destroy();

                // ── Entités ───────────────────────────────────────────────────────────────
                NkEntity CreateEntity   (const char* name = "Entity");
                NkEntity CreateMesh     (const char* name, NkStaticMesh* mesh, NkMaterial* mat,
                                        const NkVec3f& pos = {0,0,0});
                NkEntity CreateLight    (const char* name, NkLightType type = NkLightType::Point,
                                        const NkVec3f& pos = {0,1,0});
                NkEntity CreateCamera   (const char* name, const NkVec3f& pos = {0,1,5});
                NkEntity CreateParticles(const char* name, NkParticleSystem* sys,
                                        const NkVec3f& pos = {0,0,0});
                NkEntity CreateText3D   (const char* name, const char* text,
                                        NkFont* font, const NkVec3f& pos = {0,0,0});
                NkEntity CreateSkybox   (NkTextureCube* envMap);

                void DestroyEntity(NkEntity entity);
                void DestroyEntity(NkEntityID id);

                // ── Recherche ─────────────────────────────────────────────────────────────
                NkEntity Find(const char* name)   const;
                NkEntity Find(NkEntityID id)      const;
                NkEntity FindByTag(const char* tag) const;
                NkVector<NkEntity> FindAllByTag(const char* tag) const;
                NkEntity GetMainCamera()          const;

                // ── Update ────────────────────────────────────────────────────────────────
                void Update(float dt);

                // ── Rendu (prépare une NkRenderScene) ────────────────────────────────────
                void CollectRenderScene(NkRenderScene& outScene,
                                        const NkCamera& camera,
                                        NkRenderMode mode = NkRenderMode::Solid);

                // ── Sérialisation ─────────────────────────────────────────────────────────
                bool SaveToFile(const char* path) const;
                bool LoadFromFile(const char* path);

                // ── Accesseurs ────────────────────────────────────────────────────────────
                const NkString& GetName()         const { return mName; }
                void            SetName(const char* n)  { mName = n; }
                bool            IsValid()         const { return mIsValid; }
                uint32          EntityCount()     const { return (uint32)mEntityNames.Size(); }
                NkIDevice*      GetDevice()       const { return mDevice; }

                // Accès interne aux composants (utilisé par NkEntity)
                template<typename T>
                T* AddComponent(NkEntityID id);
                template<typename T>
                T* GetComponent(NkEntityID id);
                template<typename T>
                bool HasComponent(NkEntityID id) const;
                template<typename T>
                void RemoveComponent(NkEntityID id);

                // Accès aux listes de composants pour l'itération
                NkVector<NkTransformComponent*>& GetTransforms()  { return mTransforms; }
                NkVector<NkMeshComponent*>&      GetMeshes()      { return mMeshes; }
                NkVector<NkLightComponent*>&     GetLights()      { return mLights; }
                NkVector<NkCameraComponent*>&    GetCameras()     { return mCameras; }
                NkVector<NkParticleComponent*>&  GetParticles()   { return mParticles; }
                NkVector<NkTrailComponent*>&     GetTrails()      { return mTrails; }
                NkVector<NkText3DComponent*>&    GetTexts3D()     { return mTexts3D; }

            private:
                NkIDevice*   mDevice  = nullptr;
                NkString     mName;
                bool         mIsValid = false;
                NkEntityID   mNextID  = 1;

                // Métadonnées par entité
                NkUnorderedMap<NkEntityID, NkString>          mEntityNames;
                NkUnorderedMap<NkEntityID, NkString>          mEntityTags;
                NkUnorderedMap<NkEntityID, bool>              mEntityActive;
                NkUnorderedMap<NkEntityID, NkEntityID>        mEntityParent;
                NkUnorderedMap<NkEntityID, NkVector<NkEntityID>> mEntityChildren;

                // Stockage des composants (contiguë par type = cache-friendly)
                NkVector<NkTransformComponent*>        mTransforms;
                NkVector<NkMeshComponent*>             mMeshes;
                NkVector<NkLightComponent*>            mLights;
                NkVector<NkCameraComponent*>           mCameras;
                NkVector<NkParticleComponent*>         mParticles;
                NkVector<NkTrailComponent*>            mTrails;
                NkVector<NkText3DComponent*>           mTexts3D;
                NkVector<NkSkyComponent*>              mSkies;
                NkVector<NkReflectionProbeComponent*>  mProbes;
                NkVector<NkOccluderComponent*>         mOccluders;
                NkVector<NkScriptComponent*>           mScripts;

                // Maps ID → composant pour accès O(1)
                NkUnorderedMap<NkEntityID, NkTransformComponent*>       mTransformMap;
                NkUnorderedMap<NkEntityID, NkMeshComponent*>            mMeshMap;
                NkUnorderedMap<NkEntityID, NkLightComponent*>           mLightMap;
                NkUnorderedMap<NkEntityID, NkCameraComponent*>          mCameraMap;
                NkUnorderedMap<NkEntityID, NkParticleComponent*>        mParticleMap;
                NkUnorderedMap<NkEntityID, NkTrailComponent*>           mTrailMap;
                NkUnorderedMap<NkEntityID, NkText3DComponent*>          mText3DMap;
                NkUnorderedMap<NkEntityID, NkSkyComponent*>             mSkyMap;
                NkUnorderedMap<NkEntityID, NkReflectionProbeComponent*> mProbeMap;
                NkUnorderedMap<NkEntityID, NkOccluderComponent*>        mOccluderMap;
                NkUnorderedMap<NkEntityID, NkScriptComponent*>          mScriptMap;

                // Culling
                void FrustumCull(NkRenderScene& scene, const NkCamera& cam);
                void UpdateTransformHierarchy();
                void UpdateParticles(float dt);
                void UpdateScripts(float dt);
                void UpdateText3D(NkIDevice* device);
        };

        // =============================================================================
        // NkEntity template implementations
        // =============================================================================
        template<typename T, typename... Args>
        T* NkEntity::AddComponent(Args&&... args) {
            if (!IsValid()) return nullptr;
            return mScene->AddComponent<T>(mID);
        }

        template<typename T>
        T* NkEntity::GetComponent() {
            if (!IsValid()) return nullptr;
            return mScene->GetComponent<T>(mID);
        }

        template<typename T>
        const T* NkEntity::GetComponent() const {
            if (!IsValid()) return nullptr;
            return mScene->GetComponent<T>(mID);
        }

        template<typename T>
        bool NkEntity::HasComponent() const {
            if (!IsValid()) return false;
            return mScene->HasComponent<T>(mID);
        }

        // Spécialisations des templates pour les types concrets

        // Transform
        template<> inline NkTransformComponent* NkScene::AddComponent<NkTransformComponent>(NkEntityID id) {
            auto* c = new NkTransformComponent(); c->owner = id;
            mTransforms.PushBack(c); mTransformMap[id] = c; return c;
        }
        template<> inline NkTransformComponent* NkScene::GetComponent<NkTransformComponent>(NkEntityID id) {
            auto* p = mTransformMap.Find(id); return p ? *p : nullptr;
        }
        template<> inline bool NkScene::HasComponent<NkTransformComponent>(NkEntityID id) const {
            return mTransformMap.Find(id) != nullptr;
        }

        // Mesh
        template<> inline NkMeshComponent* NkScene::AddComponent<NkMeshComponent>(NkEntityID id) {
            auto* c = new NkMeshComponent(); c->owner = id;
            mMeshes.PushBack(c); mMeshMap[id] = c; return c;
        }
        template<> inline NkMeshComponent* NkScene::GetComponent<NkMeshComponent>(NkEntityID id) {
            auto* p = mMeshMap.Find(id); return p ? *p : nullptr;
        }
        template<> inline bool NkScene::HasComponent<NkMeshComponent>(NkEntityID id) const {
            return mMeshMap.Find(id) != nullptr;
        }

        // Light
        template<> inline NkLightComponent* NkScene::AddComponent<NkLightComponent>(NkEntityID id) {
            auto* c = new NkLightComponent(); c->owner = id;
            mLights.PushBack(c); mLightMap[id] = c; return c;
        }
        template<> inline NkLightComponent* NkScene::GetComponent<NkLightComponent>(NkEntityID id) {
            auto* p = mLightMap.Find(id); return p ? *p : nullptr;
        }
        template<> inline bool NkScene::HasComponent<NkLightComponent>(NkEntityID id) const {
            return mLightMap.Find(id) != nullptr;
        }

        // Camera
        template<> inline NkCameraComponent* NkScene::AddComponent<NkCameraComponent>(NkEntityID id) {
            auto* c = new NkCameraComponent(); c->owner = id;
            mCameras.PushBack(c); mCameraMap[id] = c; return c;
        }
        template<> inline NkCameraComponent* NkScene::GetComponent<NkCameraComponent>(NkEntityID id) {
            auto* p = mCameraMap.Find(id); return p ? *p : nullptr;
        }
        template<> inline bool NkScene::HasComponent<NkCameraComponent>(NkEntityID id) const {
            return mCameraMap.Find(id) != nullptr;
        }

        // NkEntity raccourcis
        inline NkTransformComponent* NkEntity::GetTransform() { return GetComponent<NkTransformComponent>(); }
        inline NkMeshComponent*      NkEntity::GetMesh()      { return GetComponent<NkMeshComponent>(); }
        inline NkLightComponent*     NkEntity::GetLight()     { return GetComponent<NkLightComponent>(); }
        inline NkCameraComponent*    NkEntity::GetCamera()    { return GetComponent<NkCameraComponent>(); }

        // =============================================================================
        // NkTransformComponent helpers
        // =============================================================================
        inline NkMat4f NkTransformComponent::ComputeLocalMatrix() const {
            NkMat4f T = NkMat4f::Translation(localPosition);
            NkMat4f Rx= NkMat4f::RotationX(NkAngle(localRotation.x));
            NkMat4f Ry= NkMat4f::RotationY(NkAngle(localRotation.y));
            NkMat4f Rz= NkMat4f::RotationZ(NkAngle(localRotation.z));
            NkMat4f S = NkMat4f::Scaling(localScale);
            return T * Ry * Rx * Rz * S;
        }

        inline void NkTransformComponent::UpdateWorldMatrix(const NkMat4f& parentWorld) {
            if (dirty) { localMatrix = ComputeLocalMatrix(); }
            worldMatrix = parentWorld * localMatrix;
            dirty = false;
        }

        inline void NkTransformComponent::LookAt(const NkVec3f& target, const NkVec3f& up) {
            NkVec3f pos = localPosition;
            NkVec3f fwd = NkVec3f(target.x-pos.x, target.y-pos.y, target.z-pos.z);
            float len = NkSqrt(fwd.x*fwd.x+fwd.y*fwd.y+fwd.z*fwd.z);
            if (len < 1e-6f) return;
            fwd = NkVec3f(fwd.x/len, fwd.y/len, fwd.z/len);
            // Euler from forward direction (approximation)
            localRotation.x = NkToDegrees(-NkAsin(fwd.y));
            localRotation.y = NkToDegrees( NkAtan2(fwd.x, fwd.z));
            dirty = true;
        }

    } // namespace renderer
} // namespace nkentseu
