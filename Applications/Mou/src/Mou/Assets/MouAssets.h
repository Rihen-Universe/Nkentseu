// =============================================================================
// Assets/MouAssets.h
// Chargeur d'assets Mú : rasterise un .svg (NkSVGCodec) OU décode un .png
// (NkImage) et l'uploade en texture NKUI (NkUICanvasBackend) -> texId pour
// NkUIDrawList::AddImage.
// =============================================================================
#pragma once

#ifndef MOU_ASSETS_H
#define MOU_ASSETS_H

#include "NKCore/NkTypes.h"

namespace nkentseu { namespace renderer { class NkUICanvasBackend; } }

namespace mou {

    class MouAssets {
    public:
        bool Init(nkentseu::renderer::NkUICanvasBackend* backend) noexcept;

        /// Charge un .svg du dossier assets/svg (rasterise à w x h). 0 si échec.
        nkentseu::uint32 LoadSvg(const char* svgName, nkentseu::int32 w, nkentseu::int32 h) noexcept;

        /// Charge un asset par chemin relatif à la racine assets/ (svg OU png).
        /// Pour un .svg : (w,h) = taille de rastérisation (0 = naturelle).
        /// Pour un .png : (w,h) ignorés (résolution native). 0 si échec.
        nkentseu::uint32 LoadAsset(const char* relPath, nkentseu::int32 w, nkentseu::int32 h) noexcept;

        /// Dimensions du dernier asset chargé (pour calculer un aspect ratio).
        nkentseu::int32 LastW() const noexcept { return mLastW; }
        nkentseu::int32 LastH() const noexcept { return mLastH; }

    private:
        nkentseu::renderer::NkUICanvasBackend* mBackend = nullptr;
        nkentseu::uint32 mNextTexId = 10000;
        nkentseu::int32  mLastW = 0, mLastH = 0;
    };

}  // namespace mou

#endif // MOU_ASSETS_H
