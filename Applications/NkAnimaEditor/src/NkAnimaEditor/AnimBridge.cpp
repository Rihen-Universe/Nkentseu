// =============================================================================
// AnimBridge.cpp — implémentation du pont. SEUL TU à inclure NKRenderer (anim).
// =============================================================================
#include "AnimBridge.h"
#include "NKRenderer/Tools/Animation/NkAnimationSystem.h"
#include "NKRenderer/Tools/Animation/NkAnimationEditor.h"
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKLogger/NkLog.h"

namespace nkanima {

    using namespace nkentseu::renderer;
    using nkentseu::math::NkMat4f;
    using nkentseu::math::NkVec4f;

    namespace {
        struct Doc {
            NkGLTFMeshData    gltf;
            NkAnimationClip   clip;
            NkAnimationPlayer player;
            NkAnimationEditor editor;
            bool              loaded  = false;
            bool              playing = true;
        };
        Doc g;
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
                logger.Info("[AnimBridge] '{0}' : {1} os, dur={2}s, {3} cles\n",
                            modelPath, (uint32)g.clip.boneTracks.Size(), g.clip.duration,
                            g.editor.PoseKeyCount());
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
        const auto& skin = g.player.GetState().boneMatrices;
        uint32 jc = (uint32)g.clip.jointInverseBind.Size();
        outPos.Resize(jc); outParent.Resize(jc);
        for (uint32 j=0;j<jc;++j) {
            // global = skin * inverse(inverseBind) ; position = colonne translation.
            NkMat4f gl = (j<(uint32)skin.Size())
                       ? (skin[j] * g.clip.jointInverseBind[j].Inverse())
                       : NkMat4f::Identity();
            outPos[j]    = { gl.position.x, gl.position.y, gl.position.z };
            outParent[j] = (j<(uint32)g.clip.jointParent.Size()) ? g.clip.jointParent[j] : -1;
        }
    }

} // namespace nkanima
