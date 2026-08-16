#pragma once
// =============================================================================
// Nogee/Shell/NogeeShell.h — chemin OPTIONNEL « coquille d'editeur » (NKEditorKit)
// =============================================================================
// ⚠️ CE CHEMIN N'EST PAS LE DEFAUT. Il ne s'active que sur `--ui=rhi`.
// Sans ce drapeau, Nogee demarre exactement comme avant : NkApplication +
// LayerStack + UILayer NKUI. Rien de ce qui marche aujourd'hui n'en depend.
//
// A quoi il sert : monter `NkEditorShell` (NKEditorKit/NKGui) et y afficher le
// panneau porte `ConsolePanelGui`, pour EXECUTER ce qui n'avait ete que LU —
// notamment le comportement du routeur d'occultation sous une surface qui
// recouvre le panneau (palette Ctrl+P, couche 50).
//
// `--occlusion-test` ajoute une sonde automatique (cf. .cpp) : sans elle, il
// faudrait cliquer a la main, ce qu'une mesure reproductible ne peut pas faire.
// =============================================================================

#include "Nogee/UkConfig.h"

namespace nkentseu {
	namespace noge {

		// Monte la coquille d'editeur et rend son code de sortie.
		// N'est appele que si cfg.uiBackend == NogeeUiBackend::RHIShell.
		int RunNogeeEditorShell(NogeAppConfig &cfg) noexcept;

		// Active la sonde d'occultation (`--occlusion-test`) : ouvre la palette
		// puis interroge le routeur, avec un TEMOIN, et ferme. A appeler AVANT
		// RunNogeeEditorShell.
		void NogeeShellEnableOcclusionProbe() noexcept;

	} // namespace noge
} // namespace nkentseu
