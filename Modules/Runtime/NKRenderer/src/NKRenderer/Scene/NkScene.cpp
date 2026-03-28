#include "NkScene.h"

#include "NKLogger/NkLog.h"
#include <algorithm>
#include <cstdio>

namespace nkentseu {
    namespace renderer {

        namespace {
            template<typename T>
            static void RemovePtr(NkVector<T*>& vec, T* ptr) {
                for (usize i = 0; i < vec.Size(); ++i) {
                    if (vec[i] == ptr) {
                        vec.Erase(vec.Begin() + i);
                        return;
                    }
                }
            }

            static float Vec3Len(const NkVec3f& v) {
                return NkSqrt(v.x * v.x + v.y * v.y + v.z * v.z);
            }

            static NkVec3f Vec3Normalize(const NkVec3f& v, const NkVec3f& fallback = {0, 1, 0}) {
                const float l = Vec3Len(v);
                if (l <= 1e-6f) return fallback;
                const float inv = 1.0f / l;
                return {v.x * inv, v.y * inv, v.z * inv};
            }
        } // namespace

        NkMaterial* NkMeshComponent::GetMaterial(uint32 idx) const {
            if (idx < (uint32)materialOverrides.Size() && materialOverrides[idx]) {
                return materialOverrides[idx];
            }
            if (mesh) {
                return mesh->GetMaterial(idx);
            }
            if (skinnedMesh) {
                return skinnedMesh->GetMaterial(idx);
            }
            return nullptr;
        }

        void NkMeshComponent::SetMaterialOverride(uint32 idx, NkMaterial* m) {
            if (idx >= (uint32)materialOverrides.Size()) {
                materialOverrides.Resize((usize)idx + 1);
            }
            materialOverrides[idx] = m;
        }

        void NkCameraComponent::SyncFromTransform(const NkTransformComponent& t) {
            camera.position = t.GetWorldPosition();
            const NkVec3f forward = Vec3Normalize(t.GetForward(), {0, 0, -1});
            const NkVec3f up = Vec3Normalize(t.GetUp(), {0, 1, 0});
            camera.target = {
                camera.position.x + forward.x,
                camera.position.y + forward.y,
                camera.position.z + forward.z
            };
            camera.up = up;
        }

        void NkEntity::SetParent(NkEntity parent) {
            if (!mScene || !IsValid()) return;
            const NkEntityID parentID = parent.IsValid() ? parent.GetID() : NK_INVALID_ENTITY;
            mScene->mEntityParent[mID] = parentID;

            if (auto* t = mScene->GetComponent<NkTransformComponent>(mID)) {
                t->parent = parentID;
            }

            if (parentID != NK_INVALID_ENTITY) {
                auto& children = mScene->mEntityChildren[parentID];
                bool exists = false;
                for (NkEntityID c : children) {
                    if (c == mID) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) children.PushBack(mID);

                if (auto* pt = mScene->GetComponent<NkTransformComponent>(parentID)) {
                    bool found = false;
                    for (NkEntityID c : pt->children) {
                        if (c == mID) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) pt->children.PushBack(mID);
                }
            }
        }

        NkEntity NkEntity::GetParent() const {
            if (!mScene || !IsValid()) return {};
            auto* p = mScene->mEntityParent.Find(mID);
            if (!p || *p == NK_INVALID_ENTITY) return {};
            return NkEntity(*p, mScene);
        }

        NkEntity NkEntity::AddChild(const char* name) {
            if (!mScene || !IsValid()) return {};
            NkEntity child = mScene->CreateEntity(name ? name : "Entity");
            child.SetParent(*this);
            return child;
        }

        NkVector<NkEntity> NkEntity::GetChildren() const {
            NkVector<NkEntity> out;
            if (!mScene || !IsValid()) return out;
            auto* p = mScene->mEntityChildren.Find(mID);
            if (!p) return out;
            for (NkEntityID childID : *p) {
                out.PushBack(NkEntity(childID, mScene));
            }
            return out;
        }

        void NkEntity::SetName(const char* name) {
            if (!mScene || !IsValid() || !name) return;
            mScene->mEntityNames[mID] = NkString(name);
        }

        const char* NkEntity::GetName() const {
            if (!mScene || !IsValid()) return "";
            auto* n = mScene->mEntityNames.Find(mID);
            return n ? n->CStr() : "";
        }

        void NkEntity::SetTag(const char* tag) {
            if (!mScene || !IsValid() || !tag) return;
            mScene->mEntityTags[mID] = NkString(tag);
        }

        const char* NkEntity::GetTag() const {
            if (!mScene || !IsValid()) return "";
            auto* t = mScene->mEntityTags.Find(mID);
            return t ? t->CStr() : "";
        }

        void NkEntity::SetActive(bool active) {
            if (!mScene || !IsValid()) return;
            mScene->mEntityActive[mID] = active;
        }

        bool NkEntity::IsActive() const {
            if (!mScene || !IsValid()) return false;
            auto* p = mScene->mEntityActive.Find(mID);
            return p ? *p : false;
        }

        void NkEntity::Destroy() {
            if (!mScene || !IsValid()) return;
            mScene->DestroyEntity(mID);
        }

        bool NkScene::Create(NkIDevice* device, const char* name) {
            Destroy();
            if (!device || !device->IsValid()) {
                return false;
            }

            mDevice = device;
            mName = name ? name : "Scene";
            mIsValid = true;
            mNextID = 1;
            return true;
        }

        void NkScene::Destroy() {
            if (!mIsValid && !mDevice) {
                return;
            }

            for (auto* c : mScripts) {
                if (c) c->OnDestroy();
            }

            for (auto* c : mTransforms) delete c;
            for (auto* c : mMeshes) delete c;
            for (auto* c : mLights) delete c;
            for (auto* c : mCameras) delete c;
            for (auto* c : mParticles) delete c;
            for (auto* c : mTrails) delete c;
            for (auto* c : mTexts3D) {
                if (c && c->extrudedMesh) {
                    delete c->extrudedMesh;
                    c->extrudedMesh = nullptr;
                }
                delete c;
            }
            for (auto* c : mSkies) delete c;
            for (auto* c : mProbes) delete c;
            for (auto* c : mOccluders) delete c;
            for (auto* c : mScripts) delete c;

            mTransforms.Clear();
            mMeshes.Clear();
            mLights.Clear();
            mCameras.Clear();
            mParticles.Clear();
            mTrails.Clear();
            mTexts3D.Clear();
            mSkies.Clear();
            mProbes.Clear();
            mOccluders.Clear();
            mScripts.Clear();

            mTransformMap.Clear();
            mMeshMap.Clear();
            mLightMap.Clear();
            mCameraMap.Clear();
            mParticleMap.Clear();
            mTrailMap.Clear();
            mText3DMap.Clear();
            mSkyMap.Clear();
            mProbeMap.Clear();
            mOccluderMap.Clear();
            mScriptMap.Clear();

            mEntityNames.Clear();
            mEntityTags.Clear();
            mEntityActive.Clear();
            mEntityParent.Clear();
            mEntityChildren.Clear();

            mNextID = 1;
            mName.Clear();
            mIsValid = false;
            mDevice = nullptr;
        }

        NkEntity NkScene::CreateEntity(const char* name) {
            if (!mIsValid) return {};

            const NkEntityID id = mNextID++;
            mEntityNames[id] = name ? NkString(name) : NkString("Entity");
            mEntityTags[id] = NkString();
            mEntityActive[id] = true;
            mEntityParent[id] = NK_INVALID_ENTITY;
            mEntityChildren[id] = {};

            AddComponent<NkTransformComponent>(id);
            return NkEntity(id, this);
        }

        NkEntity NkScene::CreateMesh(const char* name, NkStaticMesh* mesh, NkMaterial* mat, const NkVec3f& pos) {
            NkEntity e = CreateEntity(name ? name : "Mesh");
            if (!e.IsValid()) return {};

            auto* t = e.GetTransform();
            if (t) t->SetPosition(pos);

            auto* mc = AddComponent<NkMeshComponent>(e.GetID());
            mc->mesh = mesh;
            if (mat) mc->SetMaterialOverride(0, mat);
            return e;
        }

        NkEntity NkScene::CreateLight(const char* name, NkLightType type, const NkVec3f& pos) {
            NkEntity e = CreateEntity(name ? name : "Light");
            if (!e.IsValid()) return {};

            auto* t = e.GetTransform();
            if (t) t->SetPosition(pos);

            auto* lc = AddComponent<NkLightComponent>(e.GetID());
            lc->light.type = type;
            lc->light.position = pos;
            return e;
        }

        NkEntity NkScene::CreateCamera(const char* name, const NkVec3f& pos) {
            NkEntity e = CreateEntity(name ? name : "Camera");
            if (!e.IsValid()) return {};

            auto* t = e.GetTransform();
            if (t) t->SetPosition(pos);

            auto* cc = AddComponent<NkCameraComponent>(e.GetID());
            cc->isMainCamera = mCameras.Size() == 1; // the first created camera is the main one
            cc->camera.position = pos;
            cc->camera.target = {pos.x, pos.y, pos.z - 1.0f};
            return e;
        }

        NkEntity NkScene::CreateParticles(const char* name, NkParticleSystem* sys, const NkVec3f& pos) {
            NkEntity e = CreateEntity(name ? name : "Particles");
            if (!e.IsValid()) return {};

            auto* t = e.GetTransform();
            if (t) t->SetPosition(pos);

            auto* pc = AddComponent<NkParticleComponent>(e.GetID());
            pc->system = sys;
            if (pc->system) pc->system->SetPosition(pos);
            return e;
        }

        NkEntity NkScene::CreateText3D(const char* name, const char* text, NkFont* font, const NkVec3f& pos) {
            NkEntity e = CreateEntity(name ? name : "Text3D");
            if (!e.IsValid()) return {};

            auto* t = e.GetTransform();
            if (t) t->SetPosition(pos);

            auto* tc = AddComponent<NkText3DComponent>(e.GetID());
            tc->text = text ? text : "";
            tc->font = font;
            tc->dirty = true;
            return e;
        }

        NkEntity NkScene::CreateSkybox(NkTextureCube* envMap) {
            NkEntity e = CreateEntity("Sky");
            if (!e.IsValid()) return {};
            auto* sc = AddComponent<NkSkyComponent>(e.GetID());
            sc->sky.type = envMap ? NkSkySettings::Type::NK_HDRI : NkSkySettings::Type::NK_COLOR;
            sc->sky.hdri = envMap;
            return e;
        }

        void NkScene::DestroyEntity(NkEntity entity) {
            DestroyEntity(entity.GetID());
        }

        void NkScene::DestroyEntity(NkEntityID id) {
            if (id == NK_INVALID_ENTITY) return;
            if (!mEntityNames.Find(id)) return;

            auto* childs = mEntityChildren.Find(id);
            if (childs) {
                NkVector<NkEntityID> copy = *childs;
                for (NkEntityID c : copy) {
                    DestroyEntity(c);
                }
            }

            auto* parent = mEntityParent.Find(id);
            if (parent && *parent != NK_INVALID_ENTITY) {
                auto* siblings = mEntityChildren.Find(*parent);
                if (siblings) {
                    for (usize i = 0; i < siblings->Size(); ++i) {
                        if ((*siblings)[i] == id) {
                            siblings->Erase(siblings->Begin() + i);
                            break;
                        }
                    }
                }

                auto* pt = GetComponent<NkTransformComponent>(*parent);
                if (pt) {
                    for (usize i = 0; i < pt->children.Size(); ++i) {
                        if (pt->children[i] == id) {
                            pt->children.Erase(pt->children.Begin() + i);
                            break;
                        }
                    }
                }
            }

            if (auto* p = mTransformMap.Find(id)) {
                NkTransformComponent* c = *p;
                RemovePtr(mTransforms, c);
                delete c;
                mTransformMap.Erase(id);
            }
            if (auto* p = mMeshMap.Find(id)) {
                NkMeshComponent* c = *p;
                RemovePtr(mMeshes, c);
                delete c;
                mMeshMap.Erase(id);
            }
            if (auto* p = mLightMap.Find(id)) {
                NkLightComponent* c = *p;
                RemovePtr(mLights, c);
                delete c;
                mLightMap.Erase(id);
            }
            if (auto* p = mCameraMap.Find(id)) {
                NkCameraComponent* c = *p;
                RemovePtr(mCameras, c);
                delete c;
                mCameraMap.Erase(id);
            }
            if (auto* p = mParticleMap.Find(id)) {
                NkParticleComponent* c = *p;
                RemovePtr(mParticles, c);
                delete c;
                mParticleMap.Erase(id);
            }
            if (auto* p = mTrailMap.Find(id)) {
                NkTrailComponent* c = *p;
                RemovePtr(mTrails, c);
                delete c;
                mTrailMap.Erase(id);
            }
            if (auto* p = mText3DMap.Find(id)) {
                NkText3DComponent* c = *p;
                if (c && c->extrudedMesh) {
                    delete c->extrudedMesh;
                    c->extrudedMesh = nullptr;
                }
                RemovePtr(mTexts3D, c);
                delete c;
                mText3DMap.Erase(id);
            }
            if (auto* p = mSkyMap.Find(id)) {
                NkSkyComponent* c = *p;
                RemovePtr(mSkies, c);
                delete c;
                mSkyMap.Erase(id);
            }
            if (auto* p = mProbeMap.Find(id)) {
                NkReflectionProbeComponent* c = *p;
                RemovePtr(mProbes, c);
                delete c;
                mProbeMap.Erase(id);
            }
            if (auto* p = mOccluderMap.Find(id)) {
                NkOccluderComponent* c = *p;
                RemovePtr(mOccluders, c);
                delete c;
                mOccluderMap.Erase(id);
            }
            if (auto* p = mScriptMap.Find(id)) {
                NkScriptComponent* c = *p;
                if (c) c->OnDestroy();
                RemovePtr(mScripts, c);
                delete c;
                mScriptMap.Erase(id);
            }

            mEntityNames.Erase(id);
            mEntityTags.Erase(id);
            mEntityActive.Erase(id);
            mEntityParent.Erase(id);
            mEntityChildren.Erase(id);
        }

        NkEntity NkScene::Find(const char* name) const {
            if (!name) return {};
            NkEntityID found = NK_INVALID_ENTITY;
            mEntityNames.ForEach([&](const NkEntityID& id, const NkString& n) {
                if (found == NK_INVALID_ENTITY && n == name) {
                    found = id;
                }
            });
            return found != NK_INVALID_ENTITY ? NkEntity(found, const_cast<NkScene*>(this)) : NkEntity{};
        }

        NkEntity NkScene::Find(NkEntityID id) const {
            return mEntityNames.Find(id) ? NkEntity(id, const_cast<NkScene*>(this)) : NkEntity{};
        }

        NkEntity NkScene::FindByTag(const char* tag) const {
            if (!tag) return {};
            NkEntityID found = NK_INVALID_ENTITY;
            mEntityTags.ForEach([&](const NkEntityID& id, const NkString& t) {
                if (found == NK_INVALID_ENTITY && t == tag) {
                    found = id;
                }
            });
            return found != NK_INVALID_ENTITY ? NkEntity(found, const_cast<NkScene*>(this)) : NkEntity{};
        }

        NkVector<NkEntity> NkScene::FindAllByTag(const char* tag) const {
            NkVector<NkEntity> out;
            if (!tag) return out;
            mEntityTags.ForEach([&](const NkEntityID& id, const NkString& t) {
                if (t == tag) {
                    out.PushBack(NkEntity(id, const_cast<NkScene*>(this)));
                }
            });
            return out;
        }

        NkEntity NkScene::GetMainCamera() const {
            NkCameraComponent* best = nullptr;
            for (auto* cc : mCameras) {
                if (!cc || !cc->enabled) continue;
                auto* active = mEntityActive.Find(cc->owner);
                if (active && !*active) continue;
                if (cc->isMainCamera) {
                    if (!best || cc->priority > best->priority) best = cc;
                }
            }
            if (!best && !mCameras.Empty()) {
                best = mCameras[0];
            }
            return best ? NkEntity(best->owner, const_cast<NkScene*>(this)) : NkEntity{};
        }

        void NkScene::Update(float dt) {
            if (!mIsValid) return;
            UpdateTransformHierarchy();

            for (auto* c : mCameras) {
                if (!c || !c->enabled) continue;
                auto* t = GetComponent<NkTransformComponent>(c->owner);
                if (t) c->SyncFromTransform(*t);
            }

            UpdateParticles(dt);
            UpdateScripts(dt);
            UpdateText3D(mDevice);
        }

        void NkScene::CollectRenderScene(NkRenderScene& outScene, const NkCamera& camera, NkRenderMode mode) {
            outScene.Clear();
            outScene.SetCamera(camera);

            for (auto* s : mSkies) {
                if (!s || !s->enabled) continue;
                auto* active = mEntityActive.Find(s->owner);
                if (active && !*active) continue;
                outScene.SetSky(s->sky);
                break;
            }

            for (auto* lc : mLights) {
                if (!lc || !lc->enabled) continue;
                auto* active = mEntityActive.Find(lc->owner);
                if (active && !*active) continue;

                NkLight l = lc->light;
                if (auto* t = GetComponent<NkTransformComponent>(lc->owner)) {
                    l.position = t->GetWorldPosition();
                    l.direction = Vec3Normalize(t->GetForward(), {0, -1, 0});
                }
                outScene.AddLight(l);
            }

            for (auto* mc : mMeshes) {
                if (!mc || !mc->enabled || !mc->visible) continue;
                auto* active = mEntityActive.Find(mc->owner);
                if (active && !*active) continue;

                auto* t = GetComponent<NkTransformComponent>(mc->owner);
                if (!t) continue;

                NkDrawCall dc{};
                dc.mesh = mc->mesh ? mc->mesh : static_cast<NkStaticMesh*>(mc->skinnedMesh);
                dc.dynMesh = mc->dynamicMesh;
                dc.transform = t->worldMatrix;
                dc.castShadow = mc->castShadow;
                dc.receiveShadow = mc->receiveShadow;
                dc.visible = mc->visible;
                dc.lodBias = mc->lodBias;
                dc.drawOutline = mc->drawOutline;
                dc.outlineColor = mc->outlineColor;
                dc.drawWireframe = mc->drawWireframe;
                dc.drawVertices = mc->drawVertices;
                dc.drawNormals = mc->drawNormals;
                dc.normalLength = mc->normalLength;
                dc.objectID = (uint32)(mc->owner & 0xFFFFFFFFu);

                if (dc.mesh) {
                    if (!mc->materialOverrides.Empty() && mc->materialOverrides[0]) {
                        dc.material = mc->materialOverrides[0];
                    } else {
                        dc.material = dc.mesh->GetMaterial(0);
                    }
                } else if (dc.dynMesh) {
                    if (!mc->materialOverrides.Empty() && mc->materialOverrides[0]) {
                        dc.material = mc->materialOverrides[0];
                    } else {
                        dc.material = dc.dynMesh->GetMaterial();
                    }
                } else {
                    continue;
                }

                switch (mode) {
                    case NkRenderMode::NK_WIREFRAME: dc.drawWireframe = true; break;
                    case NkRenderMode::NK_POINTS:    dc.drawVertices = true; break;
                    case NkRenderMode::NK_NORMALS:   dc.drawNormals = true;  break;
                    default: break;
                }

                outScene.Submit(dc);
            }

            for (auto* tc : mTexts3D) {
                if (!tc || !tc->enabled) continue;
                auto* active = mEntityActive.Find(tc->owner);
                if (active && !*active) continue;
                if (tc->mode != NkText3DMode::NK_EXTRUDED || !tc->extrudedMesh) continue;

                auto* t = GetComponent<NkTransformComponent>(tc->owner);
                if (!t) continue;

                NkDrawCall dc{};
                dc.mesh = tc->extrudedMesh;
                dc.material = tc->extrudeMat ? tc->extrudeMat : tc->extrudedMesh->GetMaterial(0);
                dc.transform = t->worldMatrix;
                dc.objectID = (uint32)(tc->owner & 0xFFFFFFFFu);
                outScene.Submit(dc);
            }

            FrustumCull(outScene, camera);
            outScene.Sort();
        }

        bool NkScene::SaveToFile(const char* path) const {
            if (!path) return false;
            FILE* f = fopen(path, "wb");
            if (!f) return false;

            fprintf(f, "scene=%s\n", mName.CStr());
            fprintf(f, "entityCount=%u\n", (uint32)mEntityNames.Size());

            mEntityNames.ForEach([&](const NkEntityID& id, const NkString& n) {
                fprintf(f, "entity=%llu,%s\n", (unsigned long long)id, n.CStr());
                auto* t = mTransformMap.Find(id);
                if (t && *t) {
                    const NkTransformComponent* tr = *t;
                    fprintf(f, "transform=%f,%f,%f,%f,%f,%f,%f,%f,%f\n",
                        tr->localPosition.x, tr->localPosition.y, tr->localPosition.z,
                        tr->localRotation.x, tr->localRotation.y, tr->localRotation.z,
                        tr->localScale.x, tr->localScale.y, tr->localScale.z);
                }
            });

            fclose(f);
            return true;
        }

        bool NkScene::LoadFromFile(const char* path) {
            if (!path || !mDevice) return false;
            FILE* f = fopen(path, "rb");
            if (!f) return false;

            Destroy();
            if (!Create(mDevice, "Scene")) {
                fclose(f);
                return false;
            }

            char line[1024];
            NkEntity current{};
            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "scene=", 6) == 0) {
                    const char* nm = line + 6;
                    mName = NkString(nm);
                    if (!mName.Empty() && mName[mName.Size() - 1] == '\n') {
                        mName.PopBack();
                    }
                } else if (strncmp(line, "entity=", 7) == 0) {
                    char name[512] = {};
                    unsigned long long oldID = 0;
                    if (sscanf(line, "entity=%llu,%511[^\n]", &oldID, name) == 2) {
                        (void)oldID;
                        current = CreateEntity(name);
                    }
                } else if (strncmp(line, "transform=", 10) == 0 && current.IsValid()) {
                    float px, py, pz, rx, ry, rz, sx, sy, sz;
                    if (sscanf(line, "transform=%f,%f,%f,%f,%f,%f,%f,%f,%f",
                        &px, &py, &pz, &rx, &ry, &rz, &sx, &sy, &sz) == 9) {
                        auto* t = current.GetTransform();
                        if (t) {
                            t->localPosition = {px, py, pz};
                            t->localRotation = {rx, ry, rz};
                            t->localScale = {sx, sy, sz};
                            t->dirty = true;
                        }
                    }
                }
            }

            fclose(f);
            return true;
        }

        void NkScene::FrustumCull(NkRenderScene& scene, const NkCamera& cam) {
            scene.FrustumCull(cam, 16.0f / 9.0f);
        }

        void NkScene::UpdateTransformHierarchy() {
            auto updateNode = [&](auto&& self, NkEntityID id, const NkMat4f& parentWorld) -> void {
                auto* t = GetComponent<NkTransformComponent>(id);
                if (!t) return;
                t->UpdateWorldMatrix(parentWorld);

                auto* children = mEntityChildren.Find(id);
                if (!children) return;
                for (NkEntityID c : *children) {
                    self(self, c, t->worldMatrix);
                }
            };

            for (auto* t : mTransforms) {
                if (!t) continue;
                auto* active = mEntityActive.Find(t->owner);
                if (active && !*active) continue;

                auto* parent = mEntityParent.Find(t->owner);
                if (!parent || *parent == NK_INVALID_ENTITY) {
                    updateNode(updateNode, t->owner, NkMat4f::Identity());
                }
            }
        }

        void NkScene::UpdateParticles(float dt) {
            NkVector<NkEntityID> toDestroy;
            for (auto* p : mParticles) {
                if (!p || !p->enabled || !p->system) continue;
                auto* active = mEntityActive.Find(p->owner);
                if (active && !*active) continue;

                if (auto* t = GetComponent<NkTransformComponent>(p->owner)) {
                    p->system->SetPosition(t->GetWorldPosition());
                }
                p->system->Update(dt);
                p->system->UploadToGPU();
                if (p->autoDestroy && !p->system->IsAlive()) {
                    toDestroy.PushBack(p->owner);
                }
            }

            for (NkEntityID id : toDestroy) {
                DestroyEntity(id);
            }
        }

        void NkScene::UpdateScripts(float dt) {
            for (auto* s : mScripts) {
                if (!s || !s->enabled) continue;
                auto* active = mEntityActive.Find(s->owner);
                if (active && !*active) continue;
                s->OnUpdate(dt);
            }
        }

        void NkScene::UpdateText3D(NkIDevice* device) {
            if (!device) return;
            for (auto* t : mTexts3D) {
                if (!t || !t->enabled || !t->dirty) continue;
                if (t->mode != NkText3DMode::NK_EXTRUDED) {
                    t->dirty = false;
                    continue;
                }
                if (!t->font || t->text.Empty()) {
                    t->dirty = false;
                    continue;
                }

                NkMeshData md = BuildExtruded3DText(t->font, t->text.CStr(), t->extrudeParams);
                if (md.vertices.Empty()) {
                    t->dirty = false;
                    continue;
                }

                if (t->extrudedMesh) {
                    delete t->extrudedMesh;
                    t->extrudedMesh = nullptr;
                }

                NkStaticMesh* sm = new NkStaticMesh();
                if (!sm->Create(device, md)) {
                    delete sm;
                } else {
                    if (t->extrudeMat) sm->SetMaterial(0, t->extrudeMat);
                    t->extrudedMesh = sm;
                }
                t->dirty = false;
            }
        }

    } // namespace renderer
} // namespace nkentseu
