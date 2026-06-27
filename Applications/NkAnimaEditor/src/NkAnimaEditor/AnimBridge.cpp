// =============================================================================
// AnimBridge.cpp — implémentation du pont. SEUL TU à inclure NKRenderer (anim).
// =============================================================================
#include "AnimBridge.h"
#include "NKRenderer/Tools/Animation/NkAnimationSystem.h"
#include "NKRenderer/Tools/Animation/NkAnimationEditor.h"
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKLogger/NkLog.h"
#include <cstdlib>

namespace nkanima {

    using namespace nkentseu::renderer;
    using nkentseu::math::NkMat4f;
    using nkentseu::math::NkVec4f;
    using nkentseu::math::NkQuatf;

    namespace {
        struct Doc {
            NkGLTFMeshData    gltf;
            NkAnimationClip   clip;
            NkAnimationPlayer player;
            NkAnimationEditor editor;
            bool              loaded  = false;
            bool              playing = true;
            // édition de pose (§2 Pose Mode)
            NkVector<NkMat4f> bindGlobal;   // inverse(inverseBind) par joint
            NkVector<uint32>  topo;          // ordre topo (parent avant enfant)
            NkVector<NkMat4f> worldEdit;     // pose de TRAVAIL (matrices monde par joint)
            bool              editMode = false;
        };
        Doc g;

        void BuildSkeletonAux() {
            uint32 jc = (uint32)g.clip.jointInverseBind.Size();
            g.bindGlobal.Resize(jc);
            for (uint32 j=0;j<jc;++j) g.bindGlobal[j] = g.clip.jointInverseBind[j].Inverse();
            g.topo.Clear();
            NkVector<bool> placed; placed.Resize(jc); for(uint32 j=0;j<jc;++j) placed[j]=false;
            uint32 done=0,gg=0;
            while(done<jc && gg++<jc+2){ for(uint32 j=0;j<jc;++j){ if(placed[j])continue;
                int32 p=g.clip.jointParent[j]; if(p<0||placed[(uint32)p]){ g.topo.PushBack(j); placed[j]=true; ++done; } } }
            for(uint32 j=0;j<jc;++j) if(!placed[j]) g.topo.PushBack(j);
        }
    }

    bool AnimInit(const char* modelPath) {
        if (LoadGLTF(modelPath, g.gltf) && g.gltf.isSkinned) {
            int32 ai = g.gltf.animations.Empty() ? -1 : 0;
            if (g.clip.BakeFromGLTF(g.gltf, ai, 30.f)) {
                g.player.SetClip(&g.clip);
                g.player.Play(NkPlayMode::NK_LOOP, 1.f);
                g.editor.SetClip(&g.clip);
                g.editor.SetSnap(1.f / g.clip.fps);
                g.loaded = true;
                BuildSkeletonAux();
                logger.Info("[AnimBridge] '{0}' : {1} os, dur={2}s, {3} cles\n",
                            modelPath, (uint32)g.clip.boneTracks.Size(), g.clip.duration,
                            g.editor.PoseKeyCount());
                // Self-test IK-drag (NK_POSE_TEST) : tire un os feuille et vérifie qu'il
                // se rapproche de la cible.
                if (getenv("NK_POSE_TEST")) {
                    g.player.Update(0.f);
                    AnimBeginPoseEdit();
                    uint32 jc=(uint32)g.worldEdit.Size();
                    NkVector<bool> hasChild; hasChild.Resize(jc); for(uint32 j=0;j<jc;++j) hasChild[j]=false;
                    for(uint32 j=0;j<jc;++j){ int32 p=g.clip.jointParent[j]; if(p>=0) hasChild[(uint32)p]=true; }
                    int32 leaf=-1; for(uint32 j=0;j<jc;++j) if(!hasChild[j]){ leaf=(int32)j; break; }
                    if (leaf>=0) {
                        NkMat4f& m=g.worldEdit[(uint32)leaf];
                        float32 ox=m.position.x, oy=m.position.y, oz=m.position.z;
                        float32 tx=ox+0.3f, ty=oy+0.2f, tz=oz;
                        float32 d0=sqrtf((ox-tx)*(ox-tx)+(oy-ty)*(oy-ty));
                        AnimDragJoint(leaf, tx, ty, tz);
                        NkMat4f& m2=g.worldEdit[(uint32)leaf];
                        float32 d1=sqrtf((m2.position.x-tx)*(m2.position.x-tx)+(m2.position.y-ty)*(m2.position.y-ty));
                        uint32 before=g.editor.PoseKeyCount();
                        AnimCommitPoseKey();
                        logger.Info("[POSE_TEST] leaf={0} dist cible avant={1} apres={2} (rapproche:{3}) | cles {4}->{5}\n",
                                    leaf, d0, d1, (d1<d0)?1:0, before, g.editor.PoseKeyCount());
                    }
                    AnimEndPoseEdit();
                }
            }
        }
        if (!g.loaded) logger.Errorf("[AnimBridge] echec '%s'\n", modelPath);
        return g.loaded;
    }

    bool AnimLoaded() { return g.loaded; }

    void AnimUpdate(float32 dt) {
        if (g.loaded && g.playing) { g.player.Update(dt); g.editor.SetCursor(g.player.GetTime()); }
    }
    bool AnimIsPlaying() { return g.playing; }
    void AnimSetPlaying(bool p) {
        g.playing = p;
        if (p) g.player.Play(NkPlayMode::NK_LOOP, 1.f); else g.player.Pause();
    }

    float32 AnimCursor()        { return g.editor.GetCursor(); }
    void    AnimSetCursor(float32 t) { g.editor.SetCursor(t); }
    void    AnimSeek(float32 t) { g.playing=false; g.player.Pause(); g.player.SeekTo(t); g.editor.SetCursor(t); }
    float32 AnimDuration()      { return g.editor.Duration(); }
    float32 AnimFps()           { return g.editor.Fps(); }

    uint32 AnimKeyCount()       { return g.editor.PoseKeyCount(); }
    void   AnimGetKeyTimes(NkVector<float32>& out) { out = g.editor.GetPoseKeyTimes(); }
    bool   AnimIsSelected(float32 t) { return g.editor.IsSelected(t); }
    void   AnimSelectKey(float32 t)  { g.editor.SelectKey(t, false); }
    void   AnimClearSelection()      { g.editor.ClearSelection(); }
    void   AnimDeleteSelected()      { g.editor.DeleteSelected(); }
    void   AnimMoveKey(float32 a, float32 b) { g.editor.MovePoseKey(a, b); }
    void   AnimUndo()                { g.editor.Undo(); }
    void   AnimRedo()                { g.editor.Redo(); }

    void AnimInsertKeyAtCursor() {
        if (!g.loaded) return;
        float32 t = g.editor.GetCursor();
        uint32 nb = (uint32)g.clip.boneTracks.Size();
        NkVector<NkMat4f> pose; pose.Resize(nb);
        for (uint32 b=0;b<nb;++b) pose[b] = g.clip.boneTracks[b].Evaluate(t);
        g.editor.InsertPoseKey(pose);
    }

    uint32 AnimJointCount() { return (uint32)g.clip.jointInverseBind.Size(); }

    void AnimGetSkeleton(NkVector<NkVec3f>& outPos, NkVector<int32>& outParent) {
        outPos.Clear(); outParent.Clear();
        if (!g.loaded) return;
        uint32 jc = (uint32)g.clip.jointInverseBind.Size();
        outPos.Resize(jc); outParent.Resize(jc);
        const auto& skin = g.player.GetState().boneMatrices;
        for (uint32 j=0;j<jc;++j) {
            NkMat4f gl;
            if (g.editMode && j<(uint32)g.worldEdit.Size())   // pose de travail éditée
                gl = g.worldEdit[j];
            else                                              // pose du player : skin*bindGlobal
                gl = (j<(uint32)skin.Size()) ? (skin[j]*g.bindGlobal[j]) : NkMat4f::Identity();
            outPos[j]    = { gl.position.x, gl.position.y, gl.position.z };
            outParent[j] = (j<(uint32)g.clip.jointParent.Size()) ? g.clip.jointParent[j] : -1;
        }
    }

    // ── Édition de pose ──────────────────────────────────────────────────────────
    void AnimBeginPoseEdit() {
        if (!g.loaded) return;
        const auto& skin = g.player.GetState().boneMatrices;
        uint32 jc = (uint32)g.bindGlobal.Size(); g.worldEdit.Resize(jc);
        for (uint32 j=0;j<jc;++j)
            g.worldEdit[j] = (j<(uint32)skin.Size()) ? (skin[j]*g.bindGlobal[j]) : NkMat4f::Identity();
        g.editMode = true; g.playing = false; g.player.Pause();
    }
    bool AnimInPoseEdit() { return g.editMode; }
    void AnimEndPoseEdit() { g.editMode = false; }

    void AnimDragJoint(int32 jsel, float32 wx, float32 wy, float32 wz) {
        if (!g.editMode || jsel<0 || jsel>=(int32)g.worldEdit.Size()) return;
        uint32 jc = (uint32)g.worldEdit.Size();
        // Chaîne IK : jusqu'à 3 joints (… parent, jsel) racine->effecteur.
        NkVector<int32> up; { int32 c=jsel; uint32 n=0; while(c>=0&&n<3){ up.PushBack(c); c=g.clip.jointParent[(uint32)c]; ++n; } }
        NkVector<int32> chain; for(uint32 i=up.Size();i>0;--i) chain.PushBack(up[i-1]);
        uint32 n=(uint32)chain.Size(); if (n<2) return;

        NkVector<NkMat4f> baseLocal; baseLocal.Resize(jc);
        for (uint32 k=0;k<jc;++k){ int32 p=g.clip.jointParent[k];
            baseLocal[k]=(p>=0)?(g.worldEdit[(uint32)p].Inverse()*g.worldEdit[k]):g.worldEdit[k]; }

        // FABRIK (positions).
        NkVector<NkVec3f> pos; pos.Resize(n);
        for (uint32 i=0;i<n;++i){ const NkMat4f& m=g.worldEdit[(uint32)chain[i]]; pos[i]={m.position.x,m.position.y,m.position.z}; }
        NkVector<float32> len; len.Resize(n);
        for (uint32 i=1;i<n;++i){ NkVec3f d={pos[i].x-pos[i-1].x,pos[i].y-pos[i-1].y,pos[i].z-pos[i-1].z}; len[i]=sqrtf(d.x*d.x+d.y*d.y+d.z*d.z); }
        NkVec3f root=pos[0], target={wx,wy,wz};
        for (int it=0;it<16;++it){
            pos[n-1]=target;
            for (int32 i=(int32)n-2;i>=0;--i){ NkVec3f d={pos[(uint32)i].x-pos[(uint32)i+1].x,pos[(uint32)i].y-pos[(uint32)i+1].y,pos[(uint32)i].z-pos[(uint32)i+1].z};
                float32 dl=sqrtf(d.x*d.x+d.y*d.y+d.z*d.z); if(dl>1e-7f){d.x/=dl;d.y/=dl;d.z/=dl;} float32 L=len[(uint32)i+1];
                pos[(uint32)i]={pos[(uint32)i+1].x+d.x*L,pos[(uint32)i+1].y+d.y*L,pos[(uint32)i+1].z+d.z*L}; }
            pos[0]=root;
            for (uint32 i=0;i<n-1;++i){ NkVec3f d={pos[i+1].x-pos[i].x,pos[i+1].y-pos[i].y,pos[i+1].z-pos[i].z};
                float32 dl=sqrtf(d.x*d.x+d.y*d.y+d.z*d.z); if(dl>1e-7f){d.x/=dl;d.y/=dl;d.z/=dl;} float32 L=len[i+1];
                pos[i+1]={pos[i].x+d.x*L,pos[i].y+d.y*L,pos[i].z+d.z*L}; }
        }
        // aim-FK : chaîne racine->tip pivote vers les positions FABRIK.
        auto rotP=[](const NkMat4f& m){ NkMat4f r=m; r.m30=0;r.m31=0;r.m32=0;r.m33=1; return r; };
        NkVector<NkMat4f> out; out.Resize(jc);
        for (uint32 k=0;k<jc;++k) out[k]=g.worldEdit[k];
        for (uint32 i=0;i+1<n;++i){ uint32 a=(uint32)chain[i], c=(uint32)chain[i+1];
            NkMat4f childLocal=g.worldEdit[a].Inverse()*g.worldEdit[c];
            NkMat4f Ga=out[a]; NkVec3f jp={Ga.position.x,Ga.position.y,Ga.position.z};
            NkMat4f cur=Ga*childLocal; NkVec3f cp={cur.position.x,cur.position.y,cur.position.z};
            NkVec3f np=pos[i+1];
            NkVec3f va={cp.x-jp.x,cp.y-jp.y,cp.z-jp.z}, vb={np.x-jp.x,np.y-jp.y,np.z-jp.z};
            float32 al=sqrtf(va.x*va.x+va.y*va.y+va.z*va.z), bl=sqrtf(vb.x*vb.x+vb.y*vb.y+vb.z*vb.z);
            if (al>1e-6f&&bl>1e-6f){ va.x/=al;va.y/=al;va.z/=al; vb.x/=bl;vb.y/=bl;vb.z/=bl;
                NkQuatf Rw(va,vb); out[a]=NkMat4f::Translate(jp)*(Rw.ToMat4()*rotP(Ga)); }
            out[c]=out[a]*childLocal;
        }
        NkVector<bool> inChain; inChain.Resize(jc); for(uint32 k=0;k<jc;++k) inChain[k]=false;
        for (uint32 i=0;i<n;++i) inChain[(uint32)chain[i]]=true;
        for (uint32 oi=0;oi<(uint32)g.topo.Size();++oi){ uint32 k=g.topo[oi]; if(inChain[k])continue;
            int32 p=g.clip.jointParent[k]; out[k]=(p>=0)?(out[(uint32)p]*baseLocal[k]):out[k]; }
        g.worldEdit=out;
    }

    void AnimCommitPoseKey() {
        if (!g.editMode || !g.loaded) return;
        uint32 jc=(uint32)g.worldEdit.Size();
        NkVector<NkMat4f> local; local.Resize(jc);
        for (uint32 j=0;j<jc;++j){ int32 p=g.clip.jointParent[j];
            local[j]=(p>=0)?(g.worldEdit[(uint32)p].Inverse()*g.worldEdit[j]):g.worldEdit[j]; }
        g.editor.InsertPoseKey(local);
    }

} // namespace nkanima
