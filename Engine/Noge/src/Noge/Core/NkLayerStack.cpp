#include "NkLayerStack.h"

namespace nkentseu {

    NkLayerStack::~NkLayerStack() {
        for (NkLayer* layer : mLayers) {
            layer->OnDetach();
            delete layer;
        }
    }

    void NkLayerStack::PushLayer(NkLayer* layer) {
        mLayers.Insert(mLayers.Begin() + mLayerInsertIndex, layer);
        ++mLayerInsertIndex;
        layer->OnAttach();
    }

    void NkLayerStack::PushOverlay(NkOverlay* overlay) {
        mLayers.PushBack(overlay);
        overlay->OnAttach();
    }

    void NkLayerStack::PopLayer(NkLayer* layer) {
        auto it = mLayers.Find(layer);
        if (it != mLayers.End()) {
            layer->OnDetach();
            mLayers.Erase(it);
            --mLayerInsertIndex;
        }
    }

    void NkLayerStack::PopOverlay(NkOverlay* overlay) {
        auto it = mLayers.Find(overlay);
        if (it != mLayers.End()) {
            overlay->OnDetach();
            mLayers.Erase(it);
        }
    }

} // namespace nkentseu
