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

		// Active la sonde d'occultation : ouvre une surface flottante, interroge
		// la porte d'interaction depuis un VRAI panneau ancre, avec un TEMOIN
		// (meme mesure surface fermee), puis ferme. A appeler AVANT
		// RunNogeeEditorShell.
		//   prefs = false  (`--occlusion-test`)        -> palette Ctrl+P
		//   prefs = true   (`--occlusion-test-prefs`)  -> fenetre Preferences
		// UNE surface par execution : les deux peignent un voile plein ecran, les
		// ouvrir ensemble melangerait les deux mesures.
		void NogeeShellEnableOcclusionProbe(bool prefs = false) noexcept;

		// `--no-mask-body` : reproduit la condition de ConquerorLab, dont
		// `main.cpp:219` appelle `SetMaskBodyOnPopup(false)`. Sert a MESURER si ce
		// drapeau neutralise le correctif d'occlusion de la palette, au lieu de le
		// deduire d'une lecture des deux mecanismes. A appeler AVANT
		// RunNogeeEditorShell.
		void NogeeShellReproduceConquerorLabCondition() noexcept;

		// `--dragdrop-test` : sonde du glisser-deposer §7 (reparentage Outliner)
		// et §9 (carte -> Viewport). Pilote ctx.input par frames avec les rects
		// ECRAN releves par les panneaux (jamais une geometrie devinee), joue 5
		// scenarios (2 positifs, 3 negatifs), journalise chaque temoin puis
		// ferme. UNE sonde par execution : prioritaire sur --occlusion-test si
		// les deux sont passes. A appeler AVANT RunNogeeEditorShell.
		void NogeeShellEnableDragDropProbe() noexcept;

	} // namespace noge
} // namespace nkentseu
