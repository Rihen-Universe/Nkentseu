// =============================================================================
// SceneManager.cpp
// =============================================================================

#include "SceneManager.h"
#include "Scene.h"
#include "NKLogger/NkLog.h"

namespace nkentseu
{
    namespace pong
    {

        SceneManager::~SceneManager()
        {
            // Liberation defensive : si Clear() n'a pas ete appele, on
            // delete sans Exit (le contexte n'est plus disponible ici).
            for (uint32 i = 0; i < mStack.Size(); ++i)
            {
                delete mStack[i];
            }
            for (uint32 i = 0; i < mToDestroy.Size(); ++i)
            {
                delete mToDestroy[i];
            }
            if (mPendingScene != nullptr) delete mPendingScene;
        }

        // ─────────────────────────────────────────────────────────────────────
        void SceneManager::Clear(AppContext& ctx)
        {
            // Vide la pile en appelant OnExit dans l'ordre LIFO.
            while (mStack.Size() > 0)
            {
                Scene* s = mStack[mStack.Size() - 1];
                if (s != nullptr)
                {
                    s->OnExit(ctx);
                    delete s;
                }
                mStack.PopBack();
            }
            for (uint32 i = 0; i < mToDestroy.Size(); ++i)
            {
                if (mToDestroy[i] != nullptr)
                {
                    mToDestroy[i]->OnExit(ctx);
                    delete mToDestroy[i];
                }
            }
            mToDestroy.Clear();
            if (mPendingScene != nullptr)
            {
                delete mPendingScene;
                mPendingScene = nullptr;
            }
            mPendingOp = Op::None;
        }

        // ─────────────────────────────────────────────────────────────────────
        void SceneManager::Push(Scene* scene)
        {
            mPendingOp    = Op::Push;
            mPendingScene = scene;
        }

        void SceneManager::Replace(Scene* scene)
        {
            mPendingOp    = Op::Replace;
            mPendingScene = scene;
        }

        void SceneManager::Pop()
        {
            mPendingOp    = Op::Pop;
            mPendingScene = nullptr;
        }

        // ─────────────────────────────────────────────────────────────────────
        // DestroyAndCallExit — utilitaire : appelle OnExit puis libere.
        // ─────────────────────────────────────────────────────────────────────
        void SceneManager::DestroyAndCallExit(Scene* s, AppContext& ctx)
        {
            if (s == nullptr) return;
            s->OnExit(ctx);
            delete s;
        }

        // ─────────────────────────────────────────────────────────────────────
        // ApplyPending — execute l'operation en attente AVANT le frame.
        // ─────────────────────────────────────────────────────────────────────
        void SceneManager::ApplyPending(AppContext& ctx)
        {
            // 1. Detruire les scenes accumulees au frame precedent.
            for (uint32 i = 0; i < mToDestroy.Size(); ++i)
            {
                DestroyAndCallExit(mToDestroy[i], ctx);
            }
            mToDestroy.Clear();

            // 2. Executer l'operation en attente.
            switch (mPendingOp)
            {
            case Op::Push:
            {
                if (mPendingScene != nullptr)
                {
                    mStack.PushBack(mPendingScene);
                    logger.Info("[SceneManager] Push: {0}", mPendingScene->Name());
                    mPendingScene->OnEnter(ctx);
                }
                break;
            }
            case Op::Replace:
            {
                if (mStack.Size() > 0)
                {
                    Scene* old = mStack[mStack.Size() - 1];
                    mStack.PopBack();
                    mToDestroy.PushBack(old);  // detruit au prochain frame
                }
                if (mPendingScene != nullptr)
                {
                    mStack.PushBack(mPendingScene);
                    logger.Info("[SceneManager] Replace: {0}", mPendingScene->Name());
                    mPendingScene->OnEnter(ctx);
                }
                break;
            }
            case Op::Pop:
            {
                if (mStack.Size() > 0)
                {
                    Scene* old = mStack[mStack.Size() - 1];
                    mStack.PopBack();
                    logger.Info("[SceneManager] Pop: {0}", old ? old->Name() : "(null)");
                    mToDestroy.PushBack(old);
                }
                break;
            }
            default:
                break;
            }

            mPendingOp    = Op::None;
            mPendingScene = nullptr;
        }

        // ─────────────────────────────────────────────────────────────────────
        void SceneManager::Update(AppContext& ctx, float dt)
        {
            Scene* top = Top();
            if (top != nullptr) top->OnUpdate(ctx, dt);
        }

        void SceneManager::Render(AppContext& ctx)
        {
            Scene* top = Top();
            if (top != nullptr) top->OnRender(ctx);
        }

        void SceneManager::Resize(AppContext& ctx, int w, int h)
        {
            // Propage le resize a TOUTES les scenes de la pile (pas que celle
            // au sommet) pour que les overlays se recalent aussi.
            for (uint32 i = 0; i < mStack.Size(); ++i)
            {
                if (mStack[i] != nullptr) mStack[i]->OnResize(ctx, w, h);
            }
        }

        void SceneManager::Event(AppContext& ctx, NkEvent& ev)
        {
            Scene* top = Top();
            if (top != nullptr) top->OnEvent(ctx, ev);
        }

        void SceneManager::Pause(AppContext& ctx)
        {
            Scene* top = Top();
            if (top != nullptr) top->OnPause(ctx);
        }

        void SceneManager::Resume(AppContext& ctx)
        {
            Scene* top = Top();
            if (top != nullptr) top->OnResume(ctx);
        }

        Scene* SceneManager::Top() const noexcept
        {
            if (mStack.Size() == 0) return nullptr;
            return mStack[mStack.Size() - 1];
        }

        int SceneManager::Depth() const noexcept
        {
            return static_cast<int>(mStack.Size());
        }

    } // namespace pong
} // namespace nkentseu
