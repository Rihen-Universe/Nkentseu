// =============================================================================
// NkAnimationSystem.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkAnimationSystem.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include <cmath>
#include <cstring>

#ifndef NK_PI
#define NK_PI 3.14159265358979f
#endif

namespace nkentseu {
namespace renderer {

    // =========================================================================
    // NkAnimationTrack<T>::Lerp spécialisations
    // =========================================================================
    template<> float32 NkAnimationTrack<float32>::Lerp(const float32& a, const float32& b, float32 t) const {
        return a + (b-a)*t;
    }
    template<> NkVec2f NkAnimationTrack<NkVec2f>::Lerp(const NkVec2f& a, const NkVec2f& b, float32 t) const {
        return {a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t};
    }
    template<> NkVec3f NkAnimationTrack<NkVec3f>::Lerp(const NkVec3f& a, const NkVec3f& b, float32 t) const {
        return {a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t};
    }
    template<> NkVec4f NkAnimationTrack<NkVec4f>::Lerp(const NkVec4f& a, const NkVec4f& b, float32 t) const {
        // Slerp pour quaternions si besoin, lerp pour couleurs
        return {a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t,
                a.z+(b.z-a.z)*t, a.w+(b.w-a.w)*t};
    }
    template<> NkMat4f NkAnimationTrack<NkMat4f>::Lerp(const NkMat4f& a, const NkMat4f& b, float32 t) const {
        NkMat4f r;
        for(int i=0;i<4;i++) for(int j=0;j<4;j++)
            r[i][j]=a[i][j]+(b[i][j]-a[i][j])*t;
        return r;
    }
    template<> int32 NkAnimationTrack<int32>::Lerp(const int32& a, const int32& b, float32 t) const {
        return (t<0.5f)?a:b;  // step pour int (frame index)
    }

    // =========================================================================
    // NkAnimationClip helpers
    // =========================================================================
    void NkAnimationClip::RecalcDuration() {
        duration = 0.f;
        for(auto& t:boneTracks)  if(!t.Empty()) duration=fmaxf(duration,t.GetDuration());
        for(auto& t:morphTracks) if(!t.Empty()) duration=fmaxf(duration,t.GetDuration());
        if(!uvOffset.Empty())     duration=fmaxf(duration,uvOffset.GetDuration());
        if(!uvScale.Empty())      duration=fmaxf(duration,uvScale.GetDuration());
        if(!spriteFrame.Empty())  duration=fmaxf(duration,spriteFrame.GetDuration());
        if(!albedoColor.Empty())  duration=fmaxf(duration,albedoColor.GetDuration());
        if(!emissiveColor.Empty())duration=fmaxf(duration,emissiveColor.GetDuration());
        if(!position.Empty())     duration=fmaxf(duration,position.GetDuration());
        if(!rotation.Empty())     duration=fmaxf(duration,rotation.GetDuration());
        if(!scale.Empty())        duration=fmaxf(duration,scale.GetDuration());
        if(!cameraPosition.Empty())duration=fmaxf(duration,cameraPosition.GetDuration());
        if(!lightIntensity.Empty())duration=fmaxf(duration,lightIntensity.GetDuration());
        if(!ppExposure.Empty())   duration=fmaxf(duration,ppExposure.GetDuration());
    }

    void NkAnimationClip::ResizeBones(uint32 count) {
        boneCount = count;
        while((uint32)boneTracks.Size()<count) boneTracks.PushBack({});
    }

    void NkAnimationClip::AddBoneKey(uint32 boneIdx, float32 time,
                                       const NkMat4f& mat, NkInterpMode interp) {
        if(boneIdx>=(uint32)boneTracks.Size()) ResizeBones(boneIdx+1);
        boneTracks[boneIdx].AddKey(time, mat, interp);
        duration=fmaxf(duration, time);
    }

    void NkAnimationClip::BuildSpriteFlipBook(uint32 frameCount, float32 spriteFPS) {
        float32 frameDur = 1.f/spriteFPS;
        for(uint32 i=0;i<frameCount;i++)
            spriteFrame.AddKey(i*frameDur, (int32)i, NkInterpMode::NK_STEP);
        RecalcDuration();
    }

    // ── Clips prédéfinis ──────────────────────────────────────────────────────
    NkAnimationClip* NkAnimationClip::MakeSpinClip(const NkString& name,
                                                     float32 rpm, NkVec3f axis) {
        auto* c = new NkAnimationClip();
        c->name = name; c->duration = 60.f/fmaxf(rpm,0.001f);
        // Rotation complète : deux keyframes (0 → 360)
        c->rotation.AddKey(0.f,        {0,0,0,1},   NkInterpMode::NK_LINEAR);
        c->rotation.AddKey(c->duration, {0,sinf(NK_PI)*axis.y,0,cosf(NK_PI)*axis.y},
                            NkInterpMode::NK_LINEAR);
        return c;
    }

    NkAnimationClip* NkAnimationClip::MakeLightPulse(const NkString& name,
                                                       float32 minI, float32 maxI,
                                                       float32 freq) {
        auto* c = new NkAnimationClip();
        c->name = name; float32 per=1.f/fmaxf(freq,0.001f);
        c->lightIntensity.AddKey(0.f,       minI, NkInterpMode::NK_EASE_IN_OUT);
        c->lightIntensity.AddKey(per*0.5f,  maxI, NkInterpMode::NK_EASE_IN_OUT);
        c->lightIntensity.AddKey(per,        minI, NkInterpMode::NK_EASE_IN_OUT);
        c->duration=per; c->loop=true;
        return c;
    }

    NkAnimationClip* NkAnimationClip::MakeColorFade(const NkString& name,
                                                      NkVec4f from, NkVec4f to,
                                                      float32 dur, NkInterpMode interp) {
        auto* c = new NkAnimationClip();
        c->name = name; c->duration = dur;
        c->albedoColor.AddKey(0.f,   from, interp);
        c->albedoColor.AddKey(dur,   to,   interp);
        return c;
    }

    NkAnimationClip* NkAnimationClip::MakeCameraShake(const NkString& name,
                                                        float32 intensity, float32 dur,
                                                        float32 freq) {
        auto* c = new NkAnimationClip();
        c->name = name; c->duration = dur;
        uint32 steps = (uint32)(dur*freq);
        for(uint32 i=0;i<=steps;i++){
            float32 t=(float32)i/steps*dur;
            float32 env=1.f-t/dur;  // enveloppe d'atténuation
            float32 ox=(((i*12345+7)%100)/100.f*2.f-1.f)*intensity*env;
            float32 oy=(((i*23456+3)%100)/100.f*2.f-1.f)*intensity*env;
            c->cameraPosition.AddKey(t, {ox,oy,0}, NkInterpMode::NK_LINEAR);
        }
        return c;
    }

    NkAnimationClip* NkAnimationClip::MakeProceduralWalk(uint32 boneCount, float32 dur) {
        auto* c = new NkAnimationClip();
        c->name="ProceduralWalk"; c->duration=dur;
        c->ResizeBones(boneCount);
        // Animer chaque bone avec une sinusoïde décalée
        for(uint32 b=0;b<boneCount;b++){
            float32 phase=(float32)b/(boneCount>1?boneCount-1:1)*NK_PI;
            uint32 steps=16;
            for(uint32 s=0;s<=steps;s++){
                float32 t=(float32)s/steps*dur;
                float32 a=sinf(2*NK_PI*t/dur+phase)*0.3f;
                // Rotation Y simple
                NkMat4f mat;
                float32 ca=cosf(a),sa=sinf(a);
                mat=NkMat4f::Identity();
                mat[0][0]=ca; mat[2][0]=-sa;
                mat[0][2]=sa; mat[2][2]=ca;
                mat[3][1]=(float32)b*0.15f;
                c->boneTracks[b].AddKey(t, mat, NkInterpMode::NK_LINEAR);
            }
        }
        return c;
    }

    // =========================================================================
    // NkAnimationPlayer
    // =========================================================================
    void NkAnimationPlayer::SetClip(const NkAnimationClip* clip, bool autoResize) {
        mClip=clip; mTime=0.f;
        if(clip && autoResize){
            mState.boneMatrices.Resize(clip->boneCount);
            for(auto& b:mState.boneMatrices) b=NkMat4f::Identity();
            mState.morphWeights.Resize((uint32)clip->morphTracks.Size(), 0.f);
        }
    }

    void NkAnimationPlayer::Play(NkPlayMode mode, float32 speed) {
        mMode=mode; mSpeed=speed; mPlaying=true;
        for(auto& mk:mMarkers) mk.fired=false;
    }
    void NkAnimationPlayer::Pause() { mPlaying=false; }
    void NkAnimationPlayer::Stop()  { mPlaying=false; mTime=0.f; }
    void NkAnimationPlayer::SeekTo(float32 t) { mTime=t; }

    float32 NkAnimationPlayer::GetNormTime() const {
        if(!mClip||mClip->duration<=0.f) return 0.f;
        return mTime/mClip->duration;
    }
    int32 NkAnimationPlayer::GetFrame() const {
        if(!mClip) return 0;
        return (int32)(mTime*mClip->fps);
    }

    void NkAnimationPlayer::AddMarker(float32 t, const NkString& ev) {
        Marker mk; mk.time=t; mk.name=ev;
        mMarkers.PushBack(mk);
    }

    void NkAnimationPlayer::BlendTo(const NkAnimationClip* next, float32 blendDur) {
        mNextClip=next; mBlendT=blendDur; mBlendDur=blendDur;
    }

    void NkAnimationPlayer::WrapTime(float32& t, float32 dur) {
        if(dur<=0.f){t=0.f;return;}
        switch(mMode){
            case NkPlayMode::NK_LOOP:
                while(t>=dur) t-=dur;
                while(t<0.f)  t+=dur;
                break;
            case NkPlayMode::NK_PING_PONG: {
                float32 cycle=2.f*dur;
                while(t>=cycle) t-=cycle;
                if(t>dur) t=cycle-t;
                break;
            }
            case NkPlayMode::NK_CLAMP:
                if(t>dur) t=dur;
                break;
            case NkPlayMode::NK_ONCE:
                if(t>=dur){ t=dur; mPlaying=false; }
                break;
        }
    }

    void NkAnimationPlayer::Update(float32 dt) {
        if(!mClip) return;

        if(mPlaying) {
            mTime += dt * mSpeed;
            WrapTime(mTime, mClip->duration);
        }

        // Markers
        for(auto& mk:mMarkers){
            if(!mk.fired && mTime>=mk.time){
                mk.fired=true;
                if(mMarkerCb) mMarkerCb(mk.name);
            }
        }

        // Blending
        if(mBlendT>0.f && mNextClip){
            mBlendT=fmaxf(0.f, mBlendT-dt);
            float32 w=mBlendT/fmaxf(mBlendDur,0.001f);
            Evaluate(mClip,     mTime, mState);
            Evaluate(mNextClip, mTime, mBlendState);
            BlendStates(mState, mBlendState, w);
            if(mBlendT<=0.f){ mClip=mNextClip; mNextClip=nullptr; }
        } else {
            Evaluate(mClip, mTime, mState);
        }

        ComputeSpriteUV(mState, mClip);
        ComputeTransformMatrix(mState);
    }

    void NkAnimationPlayer::Evaluate(const NkAnimationClip* clip,
                                      float32 t, NkAnimationState& s) {
        if(!clip) return;
        // Bones
        s.boneMatrices.Resize(clip->boneCount);
        for(uint32 i=0;i<clip->boneCount;i++){
            if(i<(uint32)clip->boneTracks.Size() && !clip->boneTracks[i].Empty())
                s.boneMatrices[i]=clip->boneTracks[i].Evaluate(t);
            else
                s.boneMatrices[i]=NkMat4f::Identity();
        }
        // Morph
        s.morphWeights.Resize((uint32)clip->morphTracks.Size(), 0.f);
        for(uint32 i=0;i<(uint32)clip->morphTracks.Size();i++)
            s.morphWeights[i]=clip->morphTracks[i].Evaluate(t);
        // UV
        if(!clip->uvOffset.Empty())   s.uvOffset   =clip->uvOffset.Evaluate(t);
        if(!clip->uvScale.Empty())    s.uvScale    =clip->uvScale.Evaluate(t);
        if(!clip->uvRotation.Empty()) s.uvRotation =clip->uvRotation.Evaluate(t);
        if(!clip->spriteFrame.Empty())s.spriteFrame=clip->spriteFrame.Evaluate(t);
        // Matériau
        if(!clip->albedoColor.Empty())   s.albedo  =clip->albedoColor.Evaluate(t);
        if(!clip->emissiveColor.Empty()) {auto c=clip->emissiveColor.Evaluate(t); s.emissive={c.x,c.y,c.z};}
        if(!clip->emissiveStrength.Empty()) s.emissiveStrength=clip->emissiveStrength.Evaluate(t);
        if(!clip->metallic.Empty())  s.metallic =clip->metallic.Evaluate(t);
        if(!clip->roughness.Empty()) s.roughness=clip->roughness.Evaluate(t);
        if(!clip->opacity.Empty())   s.opacity  =clip->opacity.Evaluate(t);
        // Transform
        if(!clip->position.Empty()) s.position=clip->position.Evaluate(t);
        if(!clip->rotation.Empty()) s.rotation=clip->rotation.Evaluate(t);
        if(!clip->scale.Empty())    s.scale   =clip->scale.Evaluate(t);
        // Caméra
        if(!clip->cameraPosition.Empty()) s.camPos   =clip->cameraPosition.Evaluate(t);
        if(!clip->cameraTarget.Empty())   s.camTarget=clip->cameraTarget.Evaluate(t);
        if(!clip->cameraFOV.Empty())      s.camFOV   =clip->cameraFOV.Evaluate(t);
        if(!clip->cameraDOFFocus.Empty()) s.camDOFFocus=clip->cameraDOFFocus.Evaluate(t);
        if(!clip->cameraDOFAperture.Empty())s.camDOFApt=clip->cameraDOFAperture.Evaluate(t);
        // Lumière
        if(!clip->lightIntensity.Empty()){s.lightIntensity=clip->lightIntensity.Evaluate(t);}
        if(!clip->lightColor.Empty())    {s.lightColor=clip->lightColor.Evaluate(t);}
        if(!clip->lightPosition.Empty()) {s.lightPos=clip->lightPosition.Evaluate(t);}
        if(!clip->lightRange.Empty())    {s.lightRange=clip->lightRange.Evaluate(t);}
        // Post-process
        if(!clip->ppExposure.Empty())        s.ppExposure  =clip->ppExposure.Evaluate(t);
        if(!clip->ppSaturation.Empty())      s.ppSaturation=clip->ppSaturation.Evaluate(t);
        if(!clip->ppContrast.Empty())        s.ppContrast  =clip->ppContrast.Evaluate(t);
        if(!clip->ppBloomStrength.Empty())   s.ppBloom     =clip->ppBloomStrength.Evaluate(t);
        if(!clip->ppDOFFocus.Empty())        s.ppDOFFocus  =clip->ppDOFFocus.Evaluate(t);
        if(!clip->ppVignetteIntensity.Empty())s.ppVignette =clip->ppVignetteIntensity.Evaluate(t);
    }

    void NkAnimationPlayer::BlendStates(NkAnimationState& a, const NkAnimationState& b, float32 w) {
        // w=1 → a, w=0 → b
        float32 iw=1.f-w;
        for(uint32 i=0;i<(uint32)a.boneMatrices.Size()&&i<(uint32)b.boneMatrices.Size();i++){
            for(int r=0;r<4;r++) for(int c=0;c<4;c++)
                a.boneMatrices[i][r][c]=a.boneMatrices[i][r][c]*w+b.boneMatrices[i][r][c]*iw;
        }
        auto lerpf=[](float32 x,float32 y,float32 t){ return x+(y-x)*t; };
        auto lerpv3=[](NkVec3f x,NkVec3f y,float32 t)->NkVec3f{ return {x.x+(y.x-x.x)*t,x.y+(y.y-x.y)*t,x.z+(y.z-x.z)*t}; };
        a.albedo.x=lerpf(a.albedo.x,b.albedo.x,iw); a.albedo.y=lerpf(a.albedo.y,b.albedo.y,iw);
        a.albedo.z=lerpf(a.albedo.z,b.albedo.z,iw); a.albedo.w=lerpf(a.albedo.w,b.albedo.w,iw);
        a.metallic=lerpf(a.metallic,b.metallic,iw); a.roughness=lerpf(a.roughness,b.roughness,iw);
        a.opacity=lerpf(a.opacity,b.opacity,iw);
        a.position=lerpv3(a.position,b.position,iw);
        a.lightIntensity=lerpf(a.lightIntensity,b.lightIntensity,iw);
        a.ppExposure=lerpf(a.ppExposure,b.ppExposure,iw);
    }

    void NkAnimationPlayer::ComputeSpriteUV(NkAnimationState& s, const NkAnimationClip* clip) {
        if(!clip||clip->spriteAtlasCols<1||clip->spriteAtlasRows<1) return;
        uint32 f=(uint32)fmaxf(0.f,(float32)s.spriteFrame);
        uint32 total=clip->spriteAtlasCols*clip->spriteAtlasRows;
        f%=fmaxf(1u,total);
        uint32 col=f%clip->spriteAtlasCols;
        uint32 row=f/clip->spriteAtlasCols;
        float32 cw=1.f/clip->spriteAtlasCols;
        float32 rh=1.f/clip->spriteAtlasRows;
        s.spriteUV={col*cw, row*rh, (col+1)*cw, (row+1)*rh};
    }

    void NkAnimationPlayer::ComputeTransformMatrix(NkAnimationState& s) {
        // TRS : Translation * Rotation(quat) * Scale
        // Quaternion → matrice
        float32 qx=s.rotation.x,qy=s.rotation.y,qz=s.rotation.z,qw=s.rotation.w;
        float32 x2=qx*qx,y2=qy*qy,z2=qz*qz;
        NkMat4f rot=NkMat4f::Identity();
        rot[0][0]=1-2*(y2+z2); rot[1][0]=2*(qx*qy-qz*qw); rot[2][0]=2*(qx*qz+qy*qw);
        rot[0][1]=2*(qx*qy+qz*qw); rot[1][1]=1-2*(x2+z2); rot[2][1]=2*(qy*qz-qx*qw);
        rot[0][2]=2*(qx*qz-qy*qw); rot[1][2]=2*(qy*qz+qx*qw); rot[2][2]=1-2*(x2+y2);
        NkMat4f trs = NkMat4f::Translate(s.position)
                    * rot
                    * NkMat4f::Scale(s.scale);
        s.transform = trs;
    }

    // =========================================================================
    // NkAnimationSystem
    // =========================================================================
    NkAnimationSystem::~NkAnimationSystem() { Shutdown(); }

    bool NkAnimationSystem::Init(NkIDevice* device, NkRender3D* r3d) {
        mDevice=device; mR3D=r3d;
        NkComputePipelineDesc pd; pd.debugName="MorphTargets";
        mMorphCompute=mDevice->CreateComputePipeline(pd);
        return true;
    }

    void NkAnimationSystem::Shutdown() {
        for(auto* p:mPlayers) delete p; mPlayers.Clear();
        for(auto* c:mClips)   delete c; mClips.Clear();
    }

    void NkAnimationSystem::Update(float32 dt) {
        for(auto* p:mPlayers) if(p) p->Update(dt);
    }

    // ── Clips ─────────────────────────────────────────────────────────────────
    NkAnimationClip* NkAnimationSystem::CreateClip(const NkString& name) {
        auto* c=new NkAnimationClip(); c->name=name;
        mClips.PushBack(c); return c;
    }
    NkAnimationClip* NkAnimationSystem::FindClip(const NkString& name) const {
        for(auto* c:mClips) if(c&&c->name==name) return c;
        return nullptr;
    }
    void NkAnimationSystem::DestroyClip(NkAnimationClip*& c){
        for(uint32 i=0;i<(uint32)mClips.Size();i++){
            if(mClips[i]==c){delete c;mClips.Erase(mClips.Begin()+i);break;}
        }
        c=nullptr;
    }

    // ── Players ───────────────────────────────────────────────────────────────
    NkAnimationPlayer* NkAnimationSystem::CreatePlayer(const NkString& name) {
        auto* p=new NkAnimationPlayer(); p->name=name;
        mPlayers.PushBack(p); return p;
    }
    NkAnimationPlayer* NkAnimationSystem::FindPlayer(const NkString& name) const {
        for(auto* p:mPlayers) if(p&&p->name==name) return p;
        return nullptr;
    }
    void NkAnimationSystem::DestroyPlayer(NkAnimationPlayer*& p){
        for(uint32 i=0;i<(uint32)mPlayers.Size();i++){
            if(mPlayers[i]==p){delete p;mPlayers.Erase(mPlayers.Begin()+i);break;}
        }
        p=nullptr;
    }

    // ── Application au renderer ───────────────────────────────────────────────
    void NkAnimationSystem::ApplySkinnedMesh(NkMeshHandle mesh, NkMatInstHandle mat,
                                              const NkMat4f& baseWorld,
                                              const NkAnimationPlayer& player) {
        if(!mR3D) return;
        const auto& s=player.GetState();
        NkDrawCallSkinned dc;
        dc.mesh=mesh; dc.material=mat;
        dc.transform=baseWorld*s.transform;
        dc.boneMatrices=s.boneMatrices;
        dc.tint={s.albedo.x,s.albedo.y,s.albedo.z};
        dc.alpha=s.opacity;
        mR3D->SubmitSkinned(dc);
    }

    void NkAnimationSystem::ApplyOnionSkin(NkMeshHandle mesh, NkMatInstHandle mat,
                                            const NkMat4f& baseWorld,
                                            const NkAnimationPlayer& player,
                                            const int32* offsets, uint32 ghostCount,
                                            NkVec4f pastCol, NkVec4f futureCol) {
        if(!mR3D || !player.GetClip()) return;
        const NkAnimationClip* clip=player.GetClip();
        mTmpBones.Resize(clip->boneCount);

        for(uint32 g=0;g<ghostCount;g++){
            float32 t=player.GetTime()+(float32)offsets[g]/fmaxf(clip->fps,1.f);
            // Évaluer les bones à ce temps
            for(uint32 b=0;b<clip->boneCount;b++){
                if(b<(uint32)clip->boneTracks.Size()&&!clip->boneTracks[b].Empty())
                    mTmpBones[b]=clip->boneTracks[b].Evaluate(t);
                else
                    mTmpBones[b]=NkMat4f::Identity();
            }
            NkVec4f col=(offsets[g]<0)?pastCol:futureCol;
            float32 dist=(float32)abs(offsets[g])/(float32)(ghostCount/2+1);
            col.w*=(1.f-dist*0.5f);

            NkDrawCallSkinned dc;
            dc.mesh=mesh; dc.material=mat;
            dc.transform=baseWorld;
            dc.boneMatrices=mTmpBones;
            dc.tint={col.x,col.y,col.z}; dc.alpha=col.w;
            mR3D->SubmitSkinnedTinted(dc,{col.x,col.y,col.z},col.w);
        }
        // Frame courant par-dessus
        ApplySkinnedMesh(mesh,mat,baseWorld,player);
    }

    NkMat4f NkAnimationSystem::ApplyTransform(const NkAnimationPlayer& player,
                                               const NkMat4f& base) {
        return base * player.GetState().transform;
    }

    void NkAnimationSystem::ApplyMaterial(NkMaterialInstance* inst,
                                           const NkAnimationPlayer& player) {
        if(!inst) return;
        const auto& s=player.GetState();
        inst->SetAlbedo({s.albedo.x,s.albedo.y,s.albedo.z}, s.albedo.w)
             ->SetEmissive({s.emissive.x,s.emissive.y,s.emissive.z}, s.emissiveStrength)
             ->SetMetallic(s.metallic)
             ->SetRoughness(s.roughness);
    }

    void NkAnimationSystem::ApplyCamera(NkCamera3D& cam, const NkAnimationPlayer& player) {
        const auto& s=player.GetState();
        cam.SetPosition(s.camPos);
        cam.SetTarget  (s.camTarget);
        cam.SetFOV     (s.camFOV);
    }

    void NkAnimationSystem::ApplyLight(NkLightDesc& light, const NkAnimationPlayer& player) {
        const auto& s=player.GetState();
        light.intensity = s.lightIntensity;
        light.color     = s.lightColor;
        light.position  = s.lightPos;
        light.range     = s.lightRange;
    }

    void NkAnimationSystem::ApplyPostProcess(NkPostConfig& pp, const NkAnimationPlayer& player) {
        const auto& s=player.GetState();
        pp.exposure         = s.ppExposure;
        pp.saturation       = s.ppSaturation;
        pp.contrast         = s.ppContrast;
        pp.bloomStrength    = s.ppBloom;
        pp.dofFocusDist     = s.ppDOFFocus;
        pp.vignetteIntens   = s.ppVignette;
    }

    NkVec4f NkAnimationSystem::GetAnimatedSpriteUV(const NkAnimationPlayer& player) {
        return player.GetState().spriteUV;
    }

    void NkAnimationSystem::DrawSkeleton(const NkAnimationPlayer& player,
                                          const NkMat4f& baseWorld,
                                          const int32* parentIdx,
                                          NkVec4f boneColor, float32 boneSize) {
        if(!mR3D) return;
        const auto& s=player.GetState();
        uint32 n=(uint32)s.boneMatrices.Size();
        for(uint32 i=0;i<n;i++){
            NkMat4f bw=baseWorld*s.boneMatrices[i];
            NkVec3f pos={bw[3][0],bw[3][1],bw[3][2]};
            mR3D->DrawDebugSphere(pos, boneSize, boneColor);
            if(parentIdx&&parentIdx[i]>=0){
                int32 pi=parentIdx[i];
                NkMat4f pw=baseWorld*s.boneMatrices[pi];
                NkVec3f ppos={pw[3][0],pw[3][1],pw[3][2]};
                mR3D->DrawDebugLine(pos,ppos,boneColor);
            }
            mR3D->DrawDebugAxes(bw, boneSize*2.f);
        }
    }

    NkMeshHandle NkAnimationSystem::ApplyMorphTargets(NkMeshHandle base,
                                                       const NkMeshHandle* targets,
                                                       const float32* weights,
                                                       uint32 count, bool useGPU) {
        // GPU : compute shader (stub) ; CPU : retourne le mesh de base pour l'instant
        (void)targets; (void)weights; (void)count; (void)useGPU;
        return base;
    }

} // namespace renderer
} // namespace nkentseu
