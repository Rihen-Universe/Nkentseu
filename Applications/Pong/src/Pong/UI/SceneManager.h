#pragma once
// =============================================================================
// SceneManager.h
// -----------------------------------------------------------------------------
// Pile (stack) de scenes :
//   - Push(scene)     empile une scene au-dessus (overlay)
//   - Replace(scene)  remplace la scene courante (transition lineaire)
//   - Pop()           depile la scene courante (retour menu precedent)
//
// Les transitions sont differees jusqu'au debut du frame suivant pour eviter
// de detruire la scene en plein OnUpdate/OnRender.
//
// La scene au sommet de la pile est consideree comme active : elle recoit
// OnUpdate puis OnRender. Les scenes sous-jacentes sont en veille (mais
// conservees en memoire pour Pop futur).
// =============================================================================

#include "AppContext.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu
{
    class NkEvent;
}

namespace nkentseu
{
    namespace pong
    {

        class Scene;

        class SceneManager
        {
        public:
            SceneManager() = default;
            ~SceneManager();

            // ── Lifecycle ────────────────────────────────────────────────────
            /// Detache et detruit toutes les scenes (appelle OnExit pour
            /// chacune). A appeler avant Shutdown.
            void Clear(AppContext& ctx);

            // ── Operations sur la pile ───────────────────────────────────────
            /// Empile une nouvelle scene au-dessus. La scene anterieure n'est
            /// pas detruite (elle restera sous-jacente).
            void Push(Scene* scene);

            /// Remplace la scene courante par @p scene. L'ancienne est detruite
            /// au prochain frame.
            void Replace(Scene* scene);

            /// Depile la scene courante (detruite au prochain frame).
            void Pop();

            // ── Cycle de frame ───────────────────────────────────────────────
            /// Applique les operations en attente (push/replace/pop). A appeler
            /// au debut de chaque frame, avant Update.
            void ApplyPending(AppContext& ctx);

            /// Update la scene active.
            void Update(AppContext& ctx, float dt);

            /// Render la scene active.
            void Render(AppContext& ctx);

            /// Propage le resize a la scene active.
            void Resize(AppContext& ctx, int w, int h);

            /// Propage un event UI/input a la scene active uniquement (les
            /// scenes sous-jacentes ne recoivent rien).
            void Event(AppContext& ctx, NkEvent& ev);

            /// Pause / Resume forward a la scene active.
            void Pause (AppContext& ctx);
            void Resume(AppContext& ctx);

            /// Retourne la scene au sommet (ou nullptr).
            Scene* Top() const noexcept;

            /// Profondeur de la pile.
            int Depth() const noexcept;

        private:
            enum class Op { None, Push, Replace, Pop };

            // Pile de scenes (vecteur de pointeurs proprietaires).
            NkVector<Scene*> mStack;
            // Operation en attente pour le prochain frame.
            Op               mPendingOp    = Op::None;
            Scene*           mPendingScene = nullptr;
            // Scenes a detruire apres la fin du frame courant.
            NkVector<Scene*> mToDestroy;

            void DestroyAndCallExit(Scene* s, AppContext& ctx);
        };

    } // namespace pong
} // namespace nkentseu
