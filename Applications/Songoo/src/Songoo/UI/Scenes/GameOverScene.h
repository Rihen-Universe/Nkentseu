#pragma once
// GameOverScene.h
#include "Songoo/UI/Scene.h"
namespace nkentseu { namespace songoo {
    class GameOverScene : public Scene {
    public:
        GameOverScene(int winner, int s0, int s1) : mWinner(winner), mS0(s0), mS1(s1) {}
        const char* Name() const noexcept override { return "GameOver"; }
        void OnEnter (AppContext& ctx) override;
        void OnUpdate(AppContext& ctx, float dt) override;
        void OnRender(AppContext& ctx) override;
        void OnEvent (AppContext& ctx, NkEvent& ev) override;
    private:
        int   mWinner, mS0, mS1;
        float mTime = 0.f;
        // Boutons
        float mBtnReplayX=0,mBtnReplayY=0,mBtnReplayW=0,mBtnReplayH=0;
        float mBtnMenuX=0,  mBtnMenuY=0,  mBtnMenuW=0,  mBtnMenuH=0;
    };
}} // namespace
