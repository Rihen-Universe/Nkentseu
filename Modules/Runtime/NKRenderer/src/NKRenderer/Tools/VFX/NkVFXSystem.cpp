// =============================================================================
// NkVFXSystem.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkVFXSystem.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include <cmath>
#include <cstdlib>
#include <cstring>

#ifndef NK_PI
#define NK_PI 3.14159265358979f
#endif

namespace nkentseu {
    namespace renderer {

        // Pseudo-random fast [0,1]
        static inline float32 NkRandF() {
            static uint32 s=12345;
            s=s*1664525u+1013904223u;
            return (float32)(s>>8)*5.96046e-8f;
        }
        static inline float32 NkRandRange(float32 lo, float32 hi) {
            return lo + NkRandF()*(hi-lo);
        }
        static inline NkVec3f NkRandDir() {
            float32 th=NkRandF()*2*NK_PI, phi=NkRandF()*NK_PI;
            return {sinf(phi)*cosf(th), cosf(phi), sinf(phi)*sinf(th)};
        }

        // ─────────────────────────────────────────────────────────────────────────
        NkVFXSystem::~NkVFXSystem() { Shutdown(); }

        bool NkVFXSystem::Init(NkIDevice* device, NkTextureLibrary* texLib,
                                NkMeshSystem* mesh) {
            mDevice=device; mTexLib=texLib; mMesh=mesh;

            NkPipelineDesc pd;
            pd.type = NkPipelineType::NK_PARTICLES;
            pd.name = "ParticlesBillboard";
            pd.blendMode = (uint8)NkBlendMode::NK_ADDITIVE;
            pd.depthWrite = false;
            mPipeParticle = mDevice->CreatePipeline(pd);

            pd.name = "TrailMesh";
            pd.blendMode = (uint8)NkBlendMode::NK_ALPHA;
            mPipeTrail = mDevice->CreatePipeline(pd);

            pd.name = "Decal";
            pd.blendMode = (uint8)NkBlendMode::NK_ALPHA;
            pd.depthWrite = false;
            mPipeDecal = mDevice->CreatePipeline(pd);

            return true;
        }

        void NkVFXSystem::Shutdown() {
            for (auto* e : mEmitters) {
                if (e->vbo.IsValid()) mDevice->DestroyBuffer(e->vbo);
                delete e;
            }
            for (auto* t : mTrails) {
                if (t->vbo.IsValid()) mDevice->DestroyBuffer(t->vbo);
                delete t;
            }
            for (auto* d : mDecals) delete d;
            mEmitters.Clear(); mTrails.Clear(); mDecals.Clear();
        }

        // ── Émetteurs ─────────────────────────────────────────────────────────────
        NkEmitterId NkVFXSystem::CreateEmitter(const NkEmitterDesc& desc) {
            Emitter* e   = new Emitter();
            e->id        = {mNextId++};
            e->desc      = desc;
            e->particles.Resize(desc.maxParticles);
            e->enabled   = true;
            e->spawnAccum= 0.f;

            // GPU billboard VBO (NkVertexParticle)
            NkBufferDesc bd;
            bd.size  = desc.maxParticles * sizeof(NkVertexParticle) * 4; // 4 verts/particle
            bd.type  = NkBufferType::NK_VERTEX;
            bd.usage = NkBufferUsage::NK_DYNAMIC;
            e->vbo   = mDevice->CreateBuffer(bd);

            mEmitters.PushBack(e);
            return e->id;
        }

        void NkVFXSystem::DestroyEmitter(NkEmitterId& id) {
            for (uint32 i=0;i<(uint32)mEmitters.Size();i++) {
                if (mEmitters[i]->id.id == id.id) {
                    if (mEmitters[i]->vbo.IsValid())
                        mDevice->DestroyBuffer(mEmitters[i]->vbo);
                    delete mEmitters[i];
                    mEmitters.RemoveAt(i); break;
                }
            }
            id.id=0;
        }

        void NkVFXSystem::SetEmitterPos(NkEmitterId id, NkVec3f pos) {
            for (auto* e : mEmitters)
                if (e->id.id==id.id) { e->desc.position=pos; return; }
        }

        void NkVFXSystem::SetEmitterEnabled(NkEmitterId id, bool on) {
            for (auto* e : mEmitters)
                if (e->id.id==id.id) { e->enabled=on; return; }
        }

        NkEmitterDesc* NkVFXSystem::GetEmitterDesc(NkEmitterId id) {
            for (auto* e : mEmitters)
                if (e->id.id==id.id) return &e->desc;
            return nullptr;
        }

        void NkVFXSystem::Burst(NkEmitterId id, uint32 count) {
            for (auto* e : mEmitters) {
                if (e->id.id==id.id) {
                    uint32 n = (count>0) ? count : (uint32)e->desc.burstCount;
                    for (uint32 i=0;i<n;i++) SpawnParticle(e);
                    return;
                }
            }
        }

        void NkVFXSystem::SpawnParticle(Emitter* e) {
            // Trouver un slot libre
            for (auto& p : e->particles) {
                if (!p.alive) {
                    p.alive  = true;
                    p.maxLife= NkRandRange(e->desc.lifeMin, e->desc.lifeMax);
                    p.life   = p.maxLife;
                    p.size   = e->desc.sizeStart;
                    p.rotation   = NkRandF()*2*NK_PI;
                    p.rotSpeed   = NkRandRange(-2.f, 2.f);
                    p.color  = e->desc.colorStart;

                    // Position selon shape
                    switch (e->desc.shape) {
                        case NkEmitterShape::SPHERE:
                            p.pos = {e->desc.position.x + NkRandDir().x * e->desc.radius,
                                    e->desc.position.y + NkRandDir().y * e->desc.radius,
                                    e->desc.position.z + NkRandDir().z * e->desc.radius};
                            break;
                        case NkEmitterShape::BOX: {
                            NkVec3f b=e->desc.boxSize;
                            p.pos={e->desc.position.x+NkRandRange(-b.x,b.x)*0.5f,
                                    e->desc.position.y+NkRandRange(-b.y,b.y)*0.5f,
                                    e->desc.position.z+NkRandRange(-b.z,b.z)*0.5f};
                            break;
                        }
                        default: p.pos = e->desc.position; break;
                    }

                    // Vitesse
                    NkVec3f d = {
                        e->desc.velocityDir.x*(1.f-e->desc.velocityRand) + NkRandDir().x*e->desc.velocityRand,
                        e->desc.velocityDir.y*(1.f-e->desc.velocityRand) + NkRandDir().y*e->desc.velocityRand,
                        e->desc.velocityDir.z*(1.f-e->desc.velocityRand) + NkRandDir().z*e->desc.velocityRand,
                    };
                    float32 spd=NkRandRange(e->desc.speedMin,e->desc.speedMax);
                    float32 len=sqrtf(d.x*d.x+d.y*d.y+d.z*d.z);
                    if(len>1e-5f){d.x/=len;d.y/=len;d.z/=len;}
                    p.vel={d.x*spd,d.y*spd,d.z*spd};
                    e->aliveCount++;
                    return;
                }
            }
        }

        // ── Update ────────────────────────────────────────────────────────────────
        void NkVFXSystem::Update(float32 dt, const NkCamera3DData& cam) {
            mTotalParticles = 0;
            for (auto* e : mEmitters) { UpdateEmitter(e, dt, cam); mTotalParticles+=e->aliveCount; }
            for (auto* t : mTrails)   UpdateTrail(t, dt);
            // Age des decals
            for (uint32 i=0;i<(uint32)mDecals.Size();) {
                mDecals[i]->age += dt;
                if (mDecals[i]->desc.lifetime>0 && mDecals[i]->age>mDecals[i]->desc.lifetime) {
                    delete mDecals[i]; mDecals.RemoveAt(i);
                } else i++;
            }
        }

        void NkVFXSystem::UpdateEmitter(Emitter* e, float32 dt, const NkCamera3DData& cam) {
            // Spawn
            if (e->enabled && e->desc.ratePerSec > 0.f) {
                e->spawnAccum += e->desc.ratePerSec * dt;
                while (e->spawnAccum >= 1.f) { SpawnParticle(e); e->spawnAccum -= 1.f; }
            }

            e->aliveCount = 0;
            // Simuler particules actives
            for (auto& p : e->particles) {
                if (!p.alive) continue;
                p.life -= dt;
                if (p.life <= 0.f) { p.alive=false; continue; }

                // Physique
                p.vel.x += e->desc.gravity.x * dt;
                p.vel.y += e->desc.gravity.y * dt;
                p.vel.z += e->desc.gravity.z * dt;
                p.pos.x += p.vel.x * dt;
                p.pos.y += p.vel.y * dt;
                p.pos.z += p.vel.z * dt;
                p.rotation += p.rotSpeed * dt;

                // Interpolation couleur/taille
                float32 t = 1.f - (p.life/p.maxLife);
                p.color.x = e->desc.colorStart.x + (e->desc.colorEnd.x-e->desc.colorStart.x)*t;
                p.color.y = e->desc.colorStart.y + (e->desc.colorEnd.y-e->desc.colorStart.y)*t;
                p.color.z = e->desc.colorStart.z + (e->desc.colorEnd.z-e->desc.colorStart.z)*t;
                p.color.w = e->desc.colorStart.w + (e->desc.colorEnd.w-e->desc.colorStart.w)*t;
                p.size    = e->desc.sizeStart + (e->desc.sizeEnd-e->desc.sizeStart)*t;

                e->aliveCount++;
            }

            // Upload billboard VBO
            if (e->aliveCount == 0 || !e->vbo.IsValid()) return;

            NkVector<NkVertexParticle> verts;
            verts.Reserve(e->aliveCount * 4);
            for (auto& p : e->particles) {
                if (!p.alive) continue;
                uint32 col = ((uint32)(p.color.w*255)<<24)|((uint32)(p.color.z*255)<<16)|
                            ((uint32)(p.color.y*255)<<8)|(uint32)(p.color.x*255);
                // 4 verts billboard (expansés dans le vertex shader ou ici)
                NkVertexParticle v;
                v.pos=p.pos; v.size=p.size; v.rotation=p.rotation; v.color=col;
                v.uv={0,0}; verts.PushBack(v);
                v.uv={1,0}; verts.PushBack(v);
                v.uv={1,1}; verts.PushBack(v);
                v.uv={0,1}; verts.PushBack(v);
            }
            mDevice->UpdateBuffer(e->vbo, verts.Data(),
                                (uint32)verts.Size()*sizeof(NkVertexParticle));
        }

        // ── Trails ────────────────────────────────────────────────────────────────
        NkTrailId NkVFXSystem::CreateTrail(const NkTrailDesc& desc) {
            Trail* t  = new Trail();
            t->id     = {mNextId++};
            t->desc   = desc;
            NkBufferDesc bd;
            bd.size   = desc.maxPoints * sizeof(NkVertex3D) * 2; // ribbon 2 verts/point
            bd.type   = NkBufferType::NK_VERTEX;
            bd.usage  = NkBufferUsage::NK_DYNAMIC;
            t->vbo    = mDevice->CreateBuffer(bd);
            mTrails.PushBack(t);
            return t->id;
        }

        void NkVFXSystem::DestroyTrail(NkTrailId& id) {
            for (uint32 i=0;i<(uint32)mTrails.Size();i++) {
                if (mTrails[i]->id.id==id.id) {
                    if (mTrails[i]->vbo.IsValid()) mDevice->DestroyBuffer(mTrails[i]->vbo);
                    delete mTrails[i]; mTrails.RemoveAt(i); break;
                }
            }
            id.id=0;
        }

        void NkVFXSystem::AddTrailPoint(NkTrailId id, NkVec3f pos) {
            for (auto* t : mTrails) {
                if (t->id.id!=id.id) continue;
                // Ne pas ajouter si trop proche du dernier point
                if (!t->points.Empty()) {
                    auto& last = t->points[t->points.Size()-1];
                    float32 dx=pos.x-last.pos.x,dy=pos.y-last.pos.y,dz=pos.z-last.pos.z;
                    if (sqrtf(dx*dx+dy*dy+dz*dz) < t->desc.minDistance) return;
                }
                TrailPoint tp; tp.pos=pos; tp.time=0.f;
                if ((uint32)t->points.Size() >= t->desc.maxPoints)
                    t->points.RemoveAt(0);
                t->points.PushBack(tp);
                return;
            }
        }

        void NkVFXSystem::ClearTrail(NkTrailId id) {
            for (auto* t : mTrails)
                if (t->id.id==id.id) { t->points.Clear(); return; }
        }

        void NkVFXSystem::UpdateTrail(Trail* t, float32 dt) {
            // Vieillir les points
            for (auto& p : t->points) p.time += dt;
            // Supprimer points trop vieux
            while (!t->points.Empty() && t->points[0].time > t->desc.lifetime)
                t->points.RemoveAt(0);
            if (t->points.Size() < 2) return;
            // Rebuild ribbon mesh
            NkVector<NkVertex3D> verts;
            verts.Reserve(t->points.Size() * 2);
            uint32 n = (uint32)t->points.Size();
            for (uint32 i=0;i<n;i++) {
                float32 u = (float32)i/(n-1);
                float32 age = t->points[i].time/t->desc.lifetime;
                NkVec4f col = {
                    t->desc.colorStart.x+(t->desc.colorEnd.x-t->desc.colorStart.x)*age,
                    t->desc.colorStart.y+(t->desc.colorEnd.y-t->desc.colorStart.y)*age,
                    t->desc.colorStart.z+(t->desc.colorEnd.z-t->desc.colorStart.z)*age,
                    t->desc.colorStart.w+(t->desc.colorEnd.w-t->desc.colorStart.w)*age,
                };
                uint32 c=((uint32)(col.w*255)<<24)|((uint32)(col.z*255)<<16)|
                        ((uint32)(col.y*255)<<8)|(uint32)(col.x*255);
                // Direction perpendiculaire au trail (simplifiée : up world)
                NkVec3f perp={t->desc.width*0.5f,0,0};
                NkVertex3D vL,vR;
                vL.pos={t->points[i].pos.x-perp.x,t->points[i].pos.y,t->points[i].pos.z};
                vR.pos={t->points[i].pos.x+perp.x,t->points[i].pos.y,t->points[i].pos.z};
                vL.normal=vR.normal={0,0,1}; vL.tangent=vR.tangent={1,0,0};
                vL.uv={u,0}; vR.uv={u,1}; vL.color=vR.color=c;
                verts.PushBack(vL); verts.PushBack(vR);
            }
            mDevice->UpdateBuffer(t->vbo, verts.Data(), (uint32)verts.Size()*sizeof(NkVertex3D));
        }

        // ── Decals ────────────────────────────────────────────────────────────────
        NkDecalId NkVFXSystem::SpawnDecal(const NkDecalDesc& desc) {
            Decal* d = new Decal(); d->id={mNextId++}; d->desc=desc;
            mDecals.PushBack(d); return d->id;
        }

        void NkVFXSystem::DestroyDecal(NkDecalId& id) {
            for (uint32 i=0;i<(uint32)mDecals.Size();i++) {
                if (mDecals[i]->id.id==id.id) { delete mDecals[i]; mDecals.RemoveAt(i); break; }
            }
            id.id=0;
        }

        // ── Render ────────────────────────────────────────────────────────────────
        void NkVFXSystem::Render(NkICommandBuffer* cmd, const NkCamera3DData& cam) {
            for (auto* e : mEmitters) if (e->aliveCount>0) RenderEmitter(cmd,e,cam);
            for (auto* t : mTrails)   if (t->points.Size()>1) RenderTrail(cmd,t,cam);
            if (!mDecals.Empty()) RenderDecals(cmd);
        }

        void NkVFXSystem::RenderEmitter(NkICommandBuffer* cmd, Emitter* e,
                                        const NkCamera3DData& cam) {
            cmd->BindPipeline(mPipeParticle);
            if (e->desc.texture.IsValid() && mTexLib)
                cmd->BindTexture(0, mTexLib->GetRHIHandle(e->desc.texture));
            cmd->BindVertexBuffer(e->vbo, sizeof(NkVertexParticle));
            cmd->Draw(e->aliveCount*4, 1, 0, 0);
        }

        void NkVFXSystem::RenderTrail(NkICommandBuffer* cmd, Trail* t,
                                        const NkCamera3DData& cam) {
            if (t->points.Size() < 2) return;
            cmd->BindPipeline(mPipeTrail);
            if (t->desc.texture.IsValid() && mTexLib)
                cmd->BindTexture(0, mTexLib->GetRHIHandle(t->desc.texture));
            cmd->BindVertexBuffer(t->vbo, sizeof(NkVertex3D));
            uint32 n=(uint32)t->points.Size()*2;
            cmd->Draw(n, 1, 0, 0);
        }

        void NkVFXSystem::RenderDecals(NkICommandBuffer* cmd) {
            cmd->BindPipeline(mPipeDecal);
            for (auto* d : mDecals) {
                // Upload transform + opacity
                struct DecalUBO { NkMat4f transform; float32 opacity; float32 normalBlend; float32 _p[2]; } ub;
                ub.transform   = d->desc.transform;
                ub.opacity     = d->desc.opacity;
                if (d->desc.fadeOut && d->desc.lifetime > 0)
                    ub.opacity *= 1.f - (d->age/d->desc.lifetime);
                ub.normalBlend = d->desc.normalBlend;
                cmd->UpdateUniformBuffer(1, &ub, sizeof(ub));
                if (d->desc.albedo.IsValid() && mTexLib)
                    cmd->BindTexture(0, mTexLib->GetRHIHandle(d->desc.albedo));
                if (d->desc.normal.IsValid() && mTexLib)
                    cmd->BindTexture(1, mTexLib->GetRHIHandle(d->desc.normal));
                // Draw unit cube (décal projeté via shader)
                cmd->Draw(36, 1, 0, 0);
            }
        }

    } // namespace renderer
} // namespace nkentseu
