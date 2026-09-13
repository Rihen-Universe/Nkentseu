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

		// ── SONDE DU VIEWPORT (2026-09-13) ───────────────────────────────────
		// Les deux NEGATIFS de (n1) : l'entite TEMOIN_Cube existe dans les trois
		// cas, a la meme place de l'arbre de scene. `--viewport-sans-mesh` lui
		// retire son NkMeshComponent, `--viewport-inactif` lui ajoute NkInactive.
		// Une difference de pixels entre un cas et l'autre ne peut donc venir que
		// du mesh, jamais de la mise en page.
		void NogeeShellViewportSansMesh() noexcept;
		void NogeeShellViewportInactif() noexcept;
		// `--viewport-controle` : ajoute un cube soumis A LA MAIN a cote du cube
		// ECS, dans la meme scene et sur le meme chemin. Il partage toutes les
		// causes du cube ECS sauf le pont ECS : c'est ce partage qui permet de
		// designer un coupable quand le viewport est uniforme.
		// `--viewport-pointage` : aller-retour de la traduction pointeur -> rayon.
		// AUCUNE injection d'entree : la sonde APPELLE la traduction avec des
		// coordonnees ecrites. Centre + les quatre coins, avec son negatif
		// (origine decalee de 7 px), seuils poses AVANT la mesure.
		void NogeeShellViewportPointage() noexcept;
		void NogeeShellViewportControle() noexcept;
		// (n2) : la pose de camera de depart, en degres de lacet.
		void NogeeShellViewportOrbite(float32 yawDeg) noexcept;
		// Ferme la fenetre apres N images. Une mesure ne laisse pas de fenetre
		// derriere elle, et personne ne peut cliquer dans un run automatise.
		void NogeeShellViewportFermerApres(int32 frames) noexcept;
		// Ecrit la N-ieme image RENDUE du viewport dans un fichier. Ce n'est pas
		// une capture d'ecran : la cible hors ecran est relue, donc le fichier ne
		// contient QUE la scene — c'est ce qui rend deux executions comparables
		// pixel a pixel.
		void NogeeShellViewportCapture(const char *chemin, int32 numeroImage = 90) noexcept;

	} // namespace noge
} // namespace nkentseu
