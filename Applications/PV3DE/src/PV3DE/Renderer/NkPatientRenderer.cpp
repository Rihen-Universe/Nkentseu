#include "NkPatientRenderer.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NKMath.h"

using namespace nkentseu::math;

namespace nkentseu {
	namespace pv3de {

		// =====================================================================
		// STUB Phase R1 (2026-07-25) — cf. ROADMAP.md "Rendu 3D".
		// L'ancienne implémentation appelait mDevice->LoadMesh/LoadTexture,
		// NkShaderDesc::vertPath/fragPath, NkTextureLoadDesc::path/generateMips :
		// aucune de ces API n'existe sur le NkIDevice réel (RHI moderne à base de
		// pipelines/descriptor sets, pas de chargement mesh/texture par chemin).
		// La vraie couche de convenance (chargement par chemin, matériaux
		// Skin/Eye) vit dans NKRenderer, pas dans NKRHI brut — la réécriture
		// réelle est prévue Phase R3, une fois NKRenderer accessible via
		// NkApplication::GetRenderer(). En attendant, Init() échoue proprement :
		// PatientLayer::SetupRenderer() gère déjà ce cas ("mode logique seul").
		// Aucun asset 3D (.nkmesh/.png/.vert/.frag) n'existe non plus sur le
		// disque aujourd'hui (cfg.*Path ci-dessus sont des chemins visés, pas
		// des fichiers réels) — cf. ROADMAP.md.
		// =====================================================================
		bool NkPatientRenderer::Init(NkIDevice *device, NkICommandBuffer * /*cmd*/,
									 const NkPatientRenderConfig &cfg) noexcept {
			mDevice = device;
			if (!mDevice)
				return false;

			logger.Warnf("[NkPatientRenderer] Rendu GPU stubé (Phase R1) — en attente de la réécriture "
						 "NKRenderer (Phase R3). Assets visés : {} / {}\n",
						 cfg.bodyMeshPath, cfg.eyeMeshPath);

			// NkBSDriver reste initialisable (lui aussi stubé côté GPU, cf. NkBSDriver.cpp)
			mBSDriver.Init(mDevice, cfg.blendshapeCount);

			mReady = false; // pas de rendu réel tant que R3 n'est pas fait
			return false;
		}

		void NkPatientRenderer::Shutdown() noexcept {
			if (!mDevice)
				return;
			mBSDriver.Shutdown();
			mReady = false;
		}

		// =====================================================================
		void NkPatientRenderer::UpdateFromSystems(const NkFaceController &face, const NkBodyController &body,
												  const NkClinicalState &clinical) noexcept {
			// ── Blendshapes (FACS → GPU) ──────────────────────────────────────
			mBSDriver.SetWeights(face.GetBlendshapeWeights());

			// ── Paramètres skin ───────────────────────────────────────────────
			// Rougeur cutanée proportionnelle à la douleur
			float redness = NkClamp(clinical.painLevel / 10.f, 0.f, 1.f) * 0.3f;
			// Pâleur si SpO2 < 94%
			float pallor = NkClamp((94.f - clinical.spo2) / 10.f, 0.f, 0.5f);
			// Cyanose si SpO2 < 88%
			float cyanosis = NkClamp((88.f - clinical.spo2) / 10.f, 0.f, 0.8f);

			mSkinParams.skinTint =
				NkVec4f(1.f - pallor * 0.3f - cyanosis * 0.2f + redness * 0.15f, 1.f - pallor * 0.2f - cyanosis * 0.1f,
						1.f - pallor * 0.1f + cyanosis * 0.3f, 1.f);
			mSkinParams.sssStrength = 0.3f + clinical.breathingDifficulty * 0.1f;
			mSkinParams.emissiveStrength = 1.f + redness;

			// ── Paramètres yeux ───────────────────────────────────────────────
			// Pupille : mydriase (anxiété/choc) ou myosis (opioïdes/sédation)
			float anxPupil = 0.35f + clinical.anxietyLevel * 0.25f;		// mydriase anxiété
			float fatiguePupil = 0.35f - clinical.fatigueLevel * 0.15f; // myosis fatigue
			mEyeParams.pupilDiameter = NkClamp(NkLerp(fatiguePupil, anxPupil, clinical.anxietyLevel), 0.1f, 0.9f);

			// Rougeur sclère : fatigue + difficulté respiratoire
			mEyeParams.scleraRedness =
				NkClamp(clinical.fatigueLevel * 0.5f + clinical.breathingDifficulty * 0.4f, 0.f, 1.f);

			// Brillance cornée : réduite si fatigue extrême ou coma
			mEyeParams.eyeWetness = NkClamp(1.f - clinical.fatigueLevel * 0.6f, 0.f, 1.f);

			// Regard : légère déviation selon l'état
			mEyeParams.gazeOffsetX = (face.GetGazeYaw() / 45.f) * 0.5f;
			mEyeParams.gazeOffsetY = (face.GetGazePitch() / 30.f) * 0.3f;
		}

		// =====================================================================
		// Draw() — stub Phase R1. mReady est toujours false (cf. Init() ci-dessus)
		// donc ce corps ne s'exécute jamais en pratique ; conservé no-op propre
		// (plutôt que supprimé) pour que la signature/l'appelant (PatientLayer::
		// OnRender) n'aient rien à changer une fois la Phase R3 câblée.
		// =====================================================================
		void NkPatientRenderer::Draw(NkICommandBuffer *cmd, const NkMat4f & /*viewProj*/, const NkMat4f & /*model*/,
									 const NkVec3f & /*cameraPos*/) noexcept {
			if (!mReady || !cmd)
				return;
			// Phase R3 : DrawBody/DrawEyes réels via NKRenderer ici.
		}

	} // namespace pv3de
} // namespace nkentseu
