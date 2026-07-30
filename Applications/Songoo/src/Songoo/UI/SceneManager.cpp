// =============================================================================
// SceneManager.cpp — Pile de scènes avec dispatch différé
// =============================================================================

#include "SceneManager.h"
#include "Scene.h"
#include "NKLogger/NkLog.h"
#include "NKWindow/Core/NkEvent.h"

namespace nkentseu {
	namespace songoo {

		// ── API publique (enqueue des commandes) ──────────────────────────────────

		void SceneManager::Push(Scene *s) {
			mPending.PushBack({CmdType::Push, s});
		}

		void SceneManager::Pop() {
			mPending.PushBack({CmdType::Pop, nullptr});
		}

		void SceneManager::Replace(Scene *s) {
			mPending.PushBack({CmdType::Replace, s});
		}

		void SceneManager::PopToRoot() {
			mPending.PushBack({CmdType::PopToRoot, nullptr});
		}

		void SceneManager::Clear(AppContext &ctx) {
			DoClear(ctx);
		}

		// ── Dispatch ──────────────────────────────────────────────────────────────

		void SceneManager::OnUpdate(AppContext &ctx, float dt) {
			FlushPending(ctx);
			if (!mStack.IsEmpty())
				mStack.Back()->OnUpdate(ctx, dt);
			FlushPending(ctx);
		}

		void SceneManager::OnRender(AppContext &ctx) {
			if (!mStack.IsEmpty())
				mStack.Back()->OnRender(ctx);
		}

		void SceneManager::OnEvent(AppContext &ctx, NkEvent &ev) {
			if (!mStack.IsEmpty())
				mStack.Back()->OnEvent(ctx, ev);
			FlushPending(ctx);
		}

		void SceneManager::OnResize(AppContext &ctx, int w, int h) {
			(void)w;
			(void)h;
			if (!mStack.IsEmpty())
				mStack.Back()->OnResize(ctx, w, h);
		}

		void SceneManager::OnPause(AppContext &ctx) {
			if (!mStack.IsEmpty())
				mStack.Back()->OnPause(ctx);
		}

		void SceneManager::OnResume(AppContext &ctx) {
			if (!mStack.IsEmpty())
				mStack.Back()->OnResume(ctx);
		}

		// ── FlushPending ──────────────────────────────────────────────────────────

		void SceneManager::FlushPending(AppContext &ctx) {
			while (!mPending.IsEmpty()) {
				Cmd cmd = mPending.Front();
				mPending.Erase(mPending.Begin());
				switch (cmd.type) {
					case CmdType::Push:
						DoPush(ctx, cmd.scene);
						break;
					case CmdType::Pop:
						DoPop(ctx);
						break;
					case CmdType::Replace:
						DoReplace(ctx, cmd.scene);
						break;
					case CmdType::PopToRoot:
						DoPopToRoot(ctx);
						break;
					case CmdType::Clear:
						DoClear(ctx);
						break;
				}
			}
		}

		// ── Implémentations internes ──────────────────────────────────────────────

		void SceneManager::DoPush(AppContext &ctx, Scene *s) {
			logger.Info("[SceneManager] Push -> {}", s ? s->Name() : "null");
			s->OnEnter(ctx);
			mStack.PushBack(s);
		}

		void SceneManager::DoPop(AppContext &ctx) {
			if (mStack.IsEmpty())
				return;
			Scene *top = mStack.Back();
			mStack.PopBack();
			logger.Info("[SceneManager] Pop <- {}", top->Name());
			top->OnExit(ctx);
			delete top;
			if (!mStack.IsEmpty())
				mStack.Back()->OnResumedFromChild(ctx);
		}

		void SceneManager::DoReplace(AppContext &ctx, Scene *s) {
			if (!mStack.IsEmpty()) {
				Scene *top = mStack.Back();
				mStack.PopBack();
				logger.Info("[SceneManager] Replace {} -> {}", top->Name(), s ? s->Name() : "null");
				top->OnExit(ctx);
				delete top;
			}
			s->OnEnter(ctx);
			mStack.PushBack(s);
		}

		void SceneManager::DoPopToRoot(AppContext &ctx) {
			while (mStack.Size() > 1)
				DoPop(ctx);
			if (!mStack.IsEmpty())
				mStack.Back()->OnResumedFromChild(ctx);
		}

		void SceneManager::DoClear(AppContext &ctx) {
			while (!mStack.IsEmpty()) {
				Scene *top = mStack.Back();
				mStack.PopBack();
				top->OnExit(ctx);
				delete top;
			}
		}

	} // namespace songoo
} // namespace nkentseu
