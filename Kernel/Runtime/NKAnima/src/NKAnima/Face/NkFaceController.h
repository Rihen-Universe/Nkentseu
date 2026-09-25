#pragma once
// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKAnima/src/NKAnima/Face/NkFaceController.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   Le CALCUL facial : Action Units -> poids de blendshapes.
//
// POURQUOI CE FICHIER A CHANGE D'ADRESSE (Rodolf, 25/09/2026)
//   « le facial, que ce soit rig ou deformation ou calcul, c'est necessaire a
//     tout ce qui demande des personnages, des animaux, etc. qui ont des faces.
//     Donc ca doit vivre ou tous peuvent en profiter. »
//
//   Il vivait dans `Applications/PV3DE/`. Une APPLICATION est une FEUILLE du
//   graphe de dependances : rien ne peut dependre d'elle. Ce n'etait donc pas un
//   oubli de cablage si « rien ne reliait le calcul a la deformation » -- c'etait
//   une consequence de l'ADRESSE. Aucun branchement n'aurait pu la contourner.
//
//   NKAnima est SOUS NKRenderer (`NKRenderer.jenga` l. 34 declare en dependre) :
//   la deformation peut donc appeler ce calcul sans inverser aucune dependance.
//   Le sens du fil est fixe par le graphe : NKRenderer -> NKAnima, jamais
//   l'inverse.
//
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NKAnima/Face/NkActionUnit.h"
#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace anim {

		// Mapping AU → index blendshape dans le mesh 3D
		struct NkBlendshapeBinding {
				NkActionUnitId au;
				nk_uint32 blendshapeIndex; // index dans le tableau de blendshapes du mesh
				nk_float32 weight;		   // facteur multiplicatif (1 par défaut)
		};

		// =====================================================================
		// NkFaceController
		// Gère les 46 Action Units et leur mapping vers les blendshapes du mesh.
		// Met à jour les intensités via interpolation, génère le vecteur final
		// de poids blendshapes à envoyer au GPU.
		// =====================================================================
		class NkFaceController {
			public:
				NkFaceController();
				~NkFaceController() = default;

				// ── Init ──────────────────────────────────────────────────────────
				// blendshapeCount : nombre de blendshapes dans le mesh tête
				void Init(nk_uint32 blendshapeCount);

				// ── Binding AU → blendshape ───────────────────────────────────────
				void BindAU(NkActionUnitId au, nk_uint32 blendshapeIndex, nk_float32 weight = 1.f);

				// ── Pilotage ──────────────────────────────────────────────────────

				// Applique un preset FACS (tableau de paires AU/intensité)
				template <nk_usize N> void ApplyPreset(const NkAUPreset (&preset)[N], nk_float32 scale = 1.f) {
					for (nk_usize i = 0; i < N; ++i) {
						SetAUTarget(preset[i].au, preset[i].intensity * scale);
					}
				}

				// Spécialisation pour preset vide (neutre)
				void ApplyNeutral();

				// Cible directe d'un AU
				void SetAUTarget(NkActionUnitId au, nk_float32 target);

				// Intensité immédiate (sans interpolation)
				void SetAUImmediate(NkActionUnitId au, nk_float32 intensity);

				nk_float32 GetAUIntensity(NkActionUnitId au) const;

				// ── Systèmes automatiques ─────────────────────────────────────────

				// Clignement (appelé depuis Update)
				void SetBlinkEnabled(bool enabled) {
					mBlinkEnabled = enabled;
				}

				void ForceBlink(); // clignement réflexe immédiat

				// Direction du regard (yaw/pitch en degrés)
				void SetGazeDirection(nk_float32 yawDeg, nk_float32 pitchDeg);

				// Direction du regard courante (yaw/pitch en degrés, telle que clampée
				// par SetGazeDirection) — consommée par NkPatientRenderer pour l'offset
				// du regard dans le shader des yeux.
				nk_float32 GetGazeYaw() const noexcept {
					return mGazeYaw;
				}

				nk_float32 GetGazePitch() const noexcept {
					return mGazePitch;
				}

				// ── Update ────────────────────────────────────────────────────────
				void Update(nk_float32 dt);

				// ── Résultat GPU ──────────────────────────────────────────────────
				// Retourne le vecteur de poids blendshapes [0..blendshapeCount-1]
				const NkVector<nk_float32> &GetBlendshapeWeights() const {
					return mBlendshapeWeights;
				}

			private:
				void SolveBlendshapes(); // recalcule mBlendshapeWeights depuis les AUs
				void UpdateBlink(nk_float32 dt);

				// ── AUs ───────────────────────────────────────────────────────────
				static constexpr nk_uint32 kAUCount = 58; // indices 1–58
				NkActionUnit mAUs[kAUCount];

				// ── Bindings ──────────────────────────────────────────────────────
				NkVector<NkBlendshapeBinding> mBindings;

				// ── Blendshapes résultants ────────────────────────────────────────
				NkVector<nk_float32> mBlendshapeWeights;
				nk_uint32 mBlendshapeCount = 0;

				// ── Clignement ────────────────────────────────────────────────────
				bool mBlinkEnabled = true;
				nk_float32 mBlinkTimer = 0.f;
				nk_float32 mBlinkInterval = 4.f; // secondes entre clignements
				nk_float32 mBlinkPhase = 0.f;	 // 0=ouvert, 0.5=fermé, 1=ouvert
				bool mBlinking = false;

				// ── Regard ────────────────────────────────────────────────────────
				nk_float32 mGazeYaw = 0.f;
				nk_float32 mGazePitch = 0.f;
		};

	} // namespace anim
} // namespace nkentseu
