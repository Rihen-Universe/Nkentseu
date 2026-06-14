#pragma once
// =============================================================================
// NkSculptStroke.h  — NKRenderer v5.0  (Tools/PixolSculpt/)
//
// Accumule les "dabs" (tampons) le long d'un trace et maintient le DIRTY RECT :
// l'union des tuiles touchees depuis le dernier dispatch. C'est ce rectangle,
// borne par l'ecran, qui sera dispatche en compute — d'ou le cout constant en
// resolution (et non en nombre de polys). C'est l'idee centrale "ZBrush-like".
//
// ⚠️ SQUELETTE — interpolation/espacement des dabs et calcul du dirty rect
//    a implementer.
// =============================================================================
#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NKMath.h"
#include "NKRenderer/Tools/PixolSculpt/NkSculptBrush.h"
#include "NKRenderer/Tools/PixolSculpt/NkSculptTypes.h"

namespace nkentseu {
    namespace renderer {

        using namespace math;

        class NkSculptStroke {
            public:
                NkSculptStroke() noexcept = default;
                ~NkSculptStroke() noexcept = default;

                // Demarre un trace avec la brosse courante.
                void Begin(const NkSculptBrush& brush) noexcept;

                // Ajoute un echantillon (position souris/stylet). Interpole et
                // genere les dabs espaces de brush.dabSpacing * radius.
                void AddSample(const NkVec2f& screenPos, float32 pressure) noexcept;

                // Termine le trace courant.
                void End() noexcept;

                [[nodiscard]] bool IsActive() const noexcept { return mActive; }

                // Dabs en attente de dispatch (consommes par le systeme chaque frame).
                [[nodiscard]] const NkVector<NkSculptDab>& PendingDabs() const noexcept { return mPending; }

                // Rectangle de tuiles a dispatcher cette frame (working set borne).
                [[nodiscard]] NkSculptRect DirtyRect() const noexcept { return mDirty; }

                [[nodiscard]] const NkSculptBrush& Brush() const noexcept { return mBrush; }

                // Appele par le systeme APRES avoir dispatche les dabs en attente.
                void ClearPending() noexcept;

                void Reset() noexcept;

            private:
                // Etend le dirty rect pour englober la zone touchee par un dab,
                // en l'alignant sur la grille de tuiles (kNkSculptTileSize).
                void ExpandDirty(const NkSculptDab& dab) noexcept;

                NkSculptBrush         mBrush;
                NkVector<NkSculptDab> mPending;
                NkSculptRect          mDirty;
                NkVec2f               mLastSample = {0, 0};
                bool                  mActive  = false;
                bool                  mHasLast = false;
        };

    } // namespace renderer
} // namespace nkentseu
