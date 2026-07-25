#pragma once
// =============================================================================
// SceneManager.h — Pile de scènes Songo'o (Push/Pop/Replace/PopToRoot)
// Copie de la structure utilisée dans le repo Nkentseu Songoo
// =============================================================================

#include "AppContext.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	class NkEvent;
}

namespace nkentseu {
	namespace songoo {

		class Scene;

		class SceneManager {
			public:
				SceneManager() = default;
				~SceneManager() = default;

				// ── Navigation ────────────────────────────────────────────────────────

				/// Empile une nouvelle scène (l'actuelle reçoit OnPause implicitement)
				void Push(Scene *scene);

				/// Dépile la scène courante et retourne à la précédente
				void Pop();

				/// Remplace la scène courante (Pop + Push atomique)
				void Replace(Scene *scene);

				/// Revient à la scène racine (vide tout sauf la 1ère)
				void PopToRoot();

				/// Vide toute la pile (OnExit sur chaque scène)
				void Clear(AppContext &ctx);

				// ── Dispatch ──────────────────────────────────────────────────────────
				void OnUpdate(AppContext &ctx, float dt);
				void OnRender(AppContext &ctx);
				void OnEvent(AppContext &ctx, NkEvent &ev);
				void OnResize(AppContext &ctx, int w, int h);
				void OnPause(AppContext &ctx);
				void OnResume(AppContext &ctx);

			private:
				NkVector<Scene *> mStack;

				// Commandes différées (pour éviter mutation pendant dispatch)
				enum class CmdType { Push, Pop, Replace, PopToRoot, Clear };

				struct Cmd {
						CmdType type;
						Scene *scene;
				};

				NkVector<Cmd> mPending;

				void FlushPending(AppContext &ctx);
				void DoPush(AppContext &ctx, Scene *s);
				void DoPop(AppContext &ctx);
				void DoReplace(AppContext &ctx, Scene *s);
				void DoPopToRoot(AppContext &ctx);
				void DoClear(AppContext &ctx);
		};

	} // namespace songoo
} // namespace nkentseu
