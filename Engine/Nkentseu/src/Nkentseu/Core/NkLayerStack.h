#pragma once

#include "NkLayer.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {

    // =========================================================================
    // LayerStack
    // Gère une pile ordonnée de Layer et d'Overlay.
    //
    // Structure interne :
    //   [ Layer0, Layer1, ..., LayerN | Overlay0, Overlay1, ..., OverlayM ]
    //                                  ^
    //                              mLayerInsertIndex
    //
    // Layers   → insérés à gauche de mLayerInsertIndex (ordre FIFO)
    // Overlays → insérés à droite (toujours au-dessus des Layers)
    //
    // Événements : parcours de droite à gauche (Overlays en premier).
    // Updates    : parcours de gauche à droite (Layers en premier).
    // =========================================================================
    class NkLayerStack {
        public:
            NkLayerStack()  = default;
            ~NkLayerStack();

            // Ownership transféré à la stack — la stack delete à la destruction.
            void PushLayer(NkLayer* layer);
            void PushOverlay(NkOverlay* overlay);

            void PopLayer(NkLayer* layer);
            void PopOverlay(NkOverlay* overlay);

            // Itération pour update (gauche → droite)
            NkLayer** begin() { return mLayers.Begin(); }
            NkLayer** end()   { return mLayers.End(); }

            // Itération pour événements (droite → gauche)
            NkLayer** rbegin() { return mLayers.End() - 1; }
            NkLayer** rend()   { return mLayers.Begin() - 1; }

            nk_usize Size() const { return mLayers.Size(); }

        private:
            NkVector<NkLayer*> mLayers;
            nk_usize         mLayerInsertIndex = 0;
    };

} // namespace nkentseu
