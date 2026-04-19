#pragma once

#include "NKContainers/String/NkString.h"
#include "NKEvent/NkEvent.h"

namespace nkentseu {

    // =========================================================================
    // Layer
    // Unité de logique empilable dans la LayerStack.
    // Les Layers reçoivent les mises à jour du bas vers le haut.
    // Les événements se propagent du haut vers le bas ; un Layer peut
    // consommer un événement (retourner true) pour stopper la propagation.
    //
    // Usage :
    //   class GameLayer : public Layer { ... };
    //   app.PushLayer(new GameLayer("Game"));
    // =========================================================================
    class NkLayer {
        public:
            explicit NkLayer(const NkString& name = "NkLayer") : mName(name) {}
            virtual ~NkLayer() = default;

            // Appelé une fois quand la couche est ajoutée à la stack.
            virtual void OnAttach() {}

            // Appelé une fois quand la couche est retirée de la stack.
            virtual void OnDetach() {}

            // Logique — appelé chaque frame (dt en secondes).
            virtual void OnUpdate(float dt) { (void)dt; }

            // Physique — appelé à intervalle fixe (fixedDt en secondes).
            // N'est appelé que si Application::mConfig.fixedTimeStep > 0.
            virtual void OnFixedUpdate(float fixedDt) { (void)fixedDt; }

            // Rendu — appelé après tous les OnUpdate(), avant SwapBuffers.
            virtual void OnRender() {}

            // Événement — retourner true pour consommer l'événement.
            virtual bool OnEvent(NkEvent* event) { (void)event; return false; }

            // Interface ImGui / NKUI optionnelle (debug, inspector, etc.)
            virtual void OnUIRender() {}

            const NkString& GetName() const { return mName; }

        protected:
            NkString mName;
    };

    // =========================================================================
    // Overlay
    // Identique à Layer mais placé EN HAUT de la stack.
    // Utilisé pour l'UI, le debug overlay, les notifications.
    // =========================================================================
    class NkOverlay : public NkLayer {
        public:
            explicit NkOverlay(const NkString& name = "Overlay") : NkLayer(name) {}
    };

} // namespace nkentseu
