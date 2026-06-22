// =============================================================================
// SceneManager.cpp — Pile de scènes avec dispatch différé
// =============================================================================

#include "SceneManager.h"
#include "Scene.h"
#include "NKLogger/NkLog.h"
#include "NKWindow/Core/NkEvent.h"

namespace nkentseu { namespace songoo {

    // ── API publique (enqueue des commandes) ──────────────────────────────────

    void SceneManager::Push(Scene* s)    { mPending.push_back({ CmdType::Push,    s }); }
    void SceneManager::Pop()             { mPending.push_back({ CmdType::Pop,     nullptr }); }
    void SceneManager::Replace(Scene* s) { mPending.push_back({ CmdType::Replace, s }); }
    void SceneManager::PopToRoot()       { mPending.push_back({ CmdType::PopToRoot, nullptr }); }
    void SceneManager::Clear(AppContext& ctx) { DoClear(ctx); }

    // ── Dispatch ──────────────────────────────────────────────────────────────

    void SceneManager::OnUpdate(AppContext& ctx, float dt) {
        FlushPending(ctx);
        if (!mStack.empty()) mStack.back()->OnUpdate(ctx, dt);
        FlushPending(ctx);
    }

    void SceneManager::OnRender(AppContext& ctx) {
        if (!mStack.empty()) mStack.back()->OnRender(ctx);
    }

    void SceneManager::OnEvent(AppContext& ctx, NkEvent& ev) {
        if (!mStack.empty()) mStack.back()->OnEvent(ctx, ev);
        FlushPending(ctx);
    }

    void SceneManager::OnResize(AppContext& ctx, int w, int h) {
        (void)w; (void)h;
        if (!mStack.empty()) mStack.back()->OnResize(ctx, w, h);
    }

    void SceneManager::OnPause(AppContext& ctx) {
        if (!mStack.empty()) mStack.back()->OnPause(ctx);
    }

    void SceneManager::OnResume(AppContext& ctx) {
        if (!mStack.empty()) mStack.back()->OnResume(ctx);
    }

    // ── FlushPending ──────────────────────────────────────────────────────────

    void SceneManager::FlushPending(AppContext& ctx) {
        while (!mPending.empty()) {
            Cmd cmd = mPending.front();
            mPending.erase(mPending.begin());
            switch (cmd.type) {
            case CmdType::Push:      DoPush(ctx, cmd.scene);    break;
            case CmdType::Pop:       DoPop(ctx);                break;
            case CmdType::Replace:   DoReplace(ctx, cmd.scene); break;
            case CmdType::PopToRoot: DoPopToRoot(ctx);          break;
            case CmdType::Clear:     DoClear(ctx);              break;
            }
        }
    }

    // ── Implémentations internes ──────────────────────────────────────────────

    void SceneManager::DoPush(AppContext& ctx, Scene* s) {
        logger.Info("[SceneManager] Push -> {}", s ? s->Name() : "null");
        s->OnEnter(ctx);
        mStack.push_back(s);
    }

    void SceneManager::DoPop(AppContext& ctx) {
        if (mStack.empty()) return;
        Scene* top = mStack.back();
        mStack.pop_back();
        logger.Info("[SceneManager] Pop <- {}", top->Name());
        top->OnExit(ctx);
        delete top;
        if (!mStack.empty())
            mStack.back()->OnResumedFromChild(ctx);
    }

    void SceneManager::DoReplace(AppContext& ctx, Scene* s) {
        if (!mStack.empty()) {
            Scene* top = mStack.back();
            mStack.pop_back();
            logger.Info("[SceneManager] Replace {} -> {}", top->Name(), s ? s->Name() : "null");
            top->OnExit(ctx);
            delete top;
        }
        s->OnEnter(ctx);
        mStack.push_back(s);
    }

    void SceneManager::DoPopToRoot(AppContext& ctx) {
        while (mStack.size() > 1) DoPop(ctx);
        if (!mStack.empty())
            mStack.back()->OnResumedFromChild(ctx);
    }

    void SceneManager::DoClear(AppContext& ctx) {
        while (!mStack.empty()) {
            Scene* top = mStack.back();
            mStack.pop_back();
            top->OnExit(ctx);
            delete top;
        }
    }

}} // namespace nkentseu::songoo
