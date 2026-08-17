#pragma once
// =============================================================================
// Nogee/Panels/ViewportPanel.h — la zone centrale, cible du glisser-deposer §9
// =============================================================================
// CE QUE CE PANNEAU EST : le MINIMUM qui donne une cible au glisser d'assets
// (§9 « glisser une carte vers le viewport ») et occupe la zone CENTRE du dock.
//
// CE QU'IL N'EST PAS : un viewport. Le rendu de scene n'existe pas encore sur
// ce chemin — c'est MESURE, pas suppose : `ViewportLayer::RenderScene()` et
// `RenderGizmos()` etaient des TODO en toutes lettres quand la coupe NKUI a ete
// faite (commit 16732511, ROADMAP §10sexies), le viewport NKUI ne rendait RIEN.
// Ce panneau le DIT a l'ecran plutot que de le simuler.
//
// La livraison d'un asset est donc un JOURNAL (`MESURE : charge livree`) + un
// affichage du dernier chemin recu : la charge PART et SE LIVRE — ce que le
// viewport en fera (spawn dans la scene) attend le rendu de scene, et le code
// le dit au meme endroit.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKEditorKit/NkEditorPanel.h"
#include "NKEditorKit/NkEditorContext.h"

namespace nkentseu {
	namespace noge {

		class ViewportPanel final : public editorkit::NkEditorPanel {
			public:
				ViewportPanel() noexcept
					: editorkit::NkEditorPanel("Viewport", editorkit::NkEditorDockSide::NK_CENTER) {
				}

				void OnUI(editorkit::NkEditorFrameContext &ec) override;

				// ── SONDE drag-drop (--dragdrop-test) ─────────────────────────────
				void EnableProbe(bool on) noexcept {
					mProbeEnabled = on;
				}
				// Rect ECRAN reel de la zone de depot, mesure au dessin.
				bool ProbeRect(nkgui::NkRect *out) const noexcept {
					if (!mProbeRectValid)
						return false;
					if (out)
						*out = mProbeRect;
					return true;
				}
				// Dernier chemin d'asset livre ("" si aucun) + compteur de
				// livraisons — c'est ce que la sonde verifie, dans les deux sens.
				const char *LastDroppedPath() const noexcept {
					return mLastDropPath;
				}
				nk_int32 DropCount() const noexcept {
					return mDropCount;
				}

			private:
				char mLastDropPath[256] = {};
				nk_int32 mDropCount = 0;

				bool mProbeEnabled = false;
				bool mProbeRectValid = false;
				nkgui::NkRect mProbeRect{0.f, 0.f, 0.f, 0.f};
		};

	} // namespace noge
} // namespace nkentseu
