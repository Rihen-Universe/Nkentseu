#pragma once

#include "Nkentseu/Core/Application.h"
#include "Nkentseu/Core/NkApplicationConfig.h"
#include "Layers/PatientLayer.h"

namespace nkentseu {
    namespace pv3de {

        // =====================================================================
        // PatientVirtualApp
        // Application principale du Patient Virtuel 3D Emotif.
        // Hérite de Application, configure les layers PV3DE.
        // =====================================================================
        class PatientVirtualApp : public nkentseu::Application {
        public:
            explicit PatientVirtualApp(const NkApplicationConfig& config);
            ~PatientVirtualApp() override;

        protected:
            void OnInit()            override;
            void OnStart()           override;
            void OnUpdate(float dt)  override;
            void OnRender()          override;
            void OnUIRender()        override;
            void OnShutdown()        override;

        private:
            PatientLayer* mPatientLayer = nullptr;
            // TODO Phase 5+ : MedicalUILayer, ViewportLayer 3D
        };

    } // namespace pv3de
} // namespace nkentseu
