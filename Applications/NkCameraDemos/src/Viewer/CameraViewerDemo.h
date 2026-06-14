#pragma once
// =============================================================================
// CameraViewerDemo.h -- Demo 1 : visualiseur plein ecran d'une camera physique.
//
// Ouvre la camera d'index 0, affiche le flux video plein ecran dans une
// fenetre 1280x720 (texture mise a jour chaque frame). Raccourcis :
//   P     : prise de photo PNG (NkCameraSystem::SaveFrameToFile)
//   R     : demarre / arrete un enregistrement IMAGE_SEQUENCE_ONLY
//   Echap : quitte la demo
// =============================================================================

#include "NKWindow/Core/NkEntry.h"

namespace nkentseu {
    namespace cameradem {

        // Entree principale de la demo viewer. La signature reproduit nkmain
        // pour conserver state.args si la demo veut parser des sous-options.
        int RunCameraViewerDemo(const NkEntryState& state);

    } // namespace cameradem
} // namespace nkentseu
