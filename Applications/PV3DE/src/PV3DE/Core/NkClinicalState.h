#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKMath/NKMath.h"

// NOTE — NAMESPACE : NkPersonality.h, dans ce même dossier PV3DE/Core/, déclare
// nkentseu::humanoid au lieu de nkentseu::pv3de. C'est volontaire (voir le
// commentaire en tête de NkPersonality.h) — pv3de reste réservé au code
// médical spécifique à PV3DE (cet état clinique, l'émotion FSM, la respiration,
// le rendu), humanoid au framework IA humanoïde générique.
namespace nkentseu {
	namespace pv3de {

		// =====================================================================
		// NkSymptomId — identifiant unique d'un symptôme
		// =====================================================================
		using NkSymptomId = nk_uint32;
		using NkDiseaseId = nk_uint32;

		// =====================================================================
		// NkDiagnosisEntry — entrée du diagnostic différentiel
		// =====================================================================
		struct NkDiagnosisEntry {
				NkDiseaseId diseaseId = 0;
				NkString diseaseName;
				nk_float32 probability = 0.f; // 0–1
				nk_float32 severity = 0.f;	  // 0–1
		};

		// =====================================================================
		// EmotionState — états émotionnels de la FSM
		// =====================================================================
		enum class EmotionState : nk_uint8 {
			Neutral = 0,
			Discomfort,
			PainMild,
			PainSevere,
			Anxious,
			Panic,
			Nauseous,
			Exhausted,
			Confused,
			COUNT
		};

		// =====================================================================
		// NkClinicalState
		// État clinique de sortie produit par NKDiagnostic.
		// Consommé par NKEmotion, NKFace, NKBody, NKSpeech.
		// =====================================================================
		struct NkClinicalState {
				// ── Niveaux physiologiques ─────────────────────────────────────
				nk_float32 painLevel = 0.f;			  // 0–10 (EVA)
				nk_float32 nauseaLevel = 0.f;		  // 0–1
				nk_float32 fatigueLevel = 0.f;		  // 0–1
				nk_float32 anxietyLevel = 0.f;		  // 0–1
				nk_float32 breathingDifficulty = 0.f; // 0–1
				nk_float32 heartRate = 72.f;		  // BPM
				nk_float32 temperature = 37.f;		  // °C
				nk_float32 spo2 = 98.f;				  // %

				bool isConscious = true;
				bool isTrembling = false;

				// ── État émotionnel déduit ─────────────────────────────────────
				EmotionState emotionState = EmotionState::Neutral;
				nk_float32 emotionIntensity = 0.f; // 0–1

				// ── Symptômes actifs ──────────────────────────────────────────
				NkVector<NkSymptomId> activeSymptoms;

				// ── Diagnostic différentiel ───────────────────────────────────
				NkVector<NkDiagnosisEntry> differentialRanking; // trié par probabilité décroissante

				// ── Helpers ───────────────────────────────────────────────────
				bool HasSymptom(NkSymptomId id) const {
					for (auto s : activeSymptoms) {
						if (s == id)
							return true;
					}
					return false;
				}

				const NkDiagnosisEntry *GetTopDiagnosis() const {
					return differentialRanking.IsEmpty() ? nullptr : &differentialRanking[0];
				}

				// Déduit l'état émotionnel à partir des niveaux physiologiques
				//
				// NOTE — États Panic / Confused :
				//   NkEmotionFSM::Init() enregistre une transition dédiée
				//   Anxious → Panic (seuil 0.8, cf. NkEmotionFSM.cpp, commentaire
				//   "// Anxiété → panique"), et ApplyBodyParams()/ApplyVoiceParams()
				//   traitent intégralement le cas Panic (respiration, tremblement,
				//   voix). Cette transition était pourtant inatteignable en pratique
				//   car cette méthode ne produisait jamais emotionState == Panic —
				//   c'était un oubli, corrigé ci-dessous par un palier d'anxiété
				//   extrême. Confused, en revanche, n'a aucun traitement dans
				//   ApplyBodyParams/ApplyVoiceParams ni de transition dédiée : rien
				//   n'indique qu'un déclenchement physiologique automatique était
				//   prévu. Il reste volontairement accessible uniquement via
				//   NkEmotionFSM::ForceState()/ForceEmotion() (API publique de
				//   PatientLayer, et évènement de cas clinique
				//   NkCaseEventType::ForceEmotion) — utile pour scripter une
				//   confusion mentale (hypoglycémie, hypoxie, sepsis...) dont la
				//   cause n'est pas un simple niveau physiologique continu.
				void DeduceEmotion() {
					if (painLevel >= 7.f) {
						emotionState = EmotionState::PainSevere;
						emotionIntensity = (painLevel - 7.f) / 3.f;
					} else if (painLevel >= 4.f) {
						emotionState = EmotionState::PainMild;
						emotionIntensity = (painLevel - 4.f) / 3.f;
					} else if (painLevel >= 1.f) {
						emotionState = EmotionState::Discomfort;
						emotionIntensity = painLevel / 4.f;
					} else if (anxietyLevel >= 0.9f) {
						// Cf. note ci-dessus : reproduit le seuil de la transition
						// FSM Anxious → Panic (minIntensity 0.8) restée inatteignable.
						emotionState = EmotionState::Panic;
						// Qualifié explicitement (math::) : ce point n'appelait auparavant
						// aucune fonction de NKMath, donc pas de raison d'introduire ici le
						// même problème (NkClamp/NkLerp/... non qualifiés) qui empêche déjà
						// la quasi-totalité de PV3DE de compiler en l'état — cf. rapport.
						emotionIntensity = math::NkClamp((anxietyLevel - 0.9f) / 0.1f, 0.f, 1.f);
					} else if (anxietyLevel >= 0.7f) {
						emotionState = EmotionState::Anxious;
						emotionIntensity = anxietyLevel;
					} else if (nauseaLevel >= 0.5f) {
						emotionState = EmotionState::Nauseous;
						emotionIntensity = nauseaLevel;
					} else if (fatigueLevel >= 0.6f) {
						emotionState = EmotionState::Exhausted;
						emotionIntensity = fatigueLevel;
					} else {
						emotionState = EmotionState::Neutral;
						emotionIntensity = 0.f;
					}
				}
		};

	} // namespace pv3de
} // namespace nkentseu
