// =============================================================================
// NKRenderer/Tools/Director/NkRoleContext.h
// -----------------------------------------------------------------------------
// M4bis brique 1/5 (NkAnima — couche acteur / directeur IA) : CONTEXTE DE RÔLE.
//
// Rappel du principe M4bis (ROADMAP.md) : le modèle de langage reste HORS de la
// boucle temps réel (appelé une fois par « beat » de scène), et sa sortie doit
// être STRUCTURÉE et VALIDÉE PAR SCHÉMA (jamais du texte libre injecté tel quel
// dans le pipeline d'animation). Ce module fournit :
//   - la structure de donnée du rôle joué par un personnage (nom, traits de
//     personnalité, état émotionnel courant, objectif de scène, historique
//     court d'événements) ;
//   - la sérialisation (NKSerialization : NkArchive <-> JSON) ;
//   - un VALIDATEUR DE SCHÉMA STRICT (`NkRoleContextSchema::Validate`) qui
//     rejette toute archive/JSON mal formé (champ manquant, type incorrect,
//     valeur hors bornes, émotion inconnue) — c'est la porte d'entrée qui
//     empêchera une sortie IA malformée d'atteindre le traducteur de
//     performance (brique 3/5, future).
//
// Pure Foundation (NKContainers + NKSerialization) : AUCUN GPU, AUCUN appel
// réseau/LLM ici (le pont directeur — brique 2/5 — est une étape séparée qui
// PRODUIRA ce genre d'archive). Testable headless (`NkRoleContext::SelfTest`).
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKSerialization/NkArchive.h"

namespace nkentseu {
	namespace renderer {

		// =========================================================================
		// NkEmotion — palette d'émotions de base (démarrage M4bis : 6 émotions,
		// cf. roadmap bullet "Traducteur de performance" : "5-6 émotions -> 5-6
		// profils de blend"). Extensible plus tard sans casser le schéma (ajouter
		// des valeurs en fin d'énum + dans les tables de conversion nom<->enum).
		// =========================================================================
		enum class NkEmotion : uint8 {
			NK_EMOTION_NEUTRAL = 0,
			NK_EMOTION_JOY,
			NK_EMOTION_SADNESS,
			NK_EMOTION_ANGER,
			NK_EMOTION_FEAR,
			NK_EMOTION_SURPRISE,
			NK_EMOTION_DISGUST,
			NK_EMOTION_COUNT
		};

		// Nom canonique (minuscules, stable dans le JSON) d'une émotion. "?" si
		// hors bornes (ne doit jamais arriver sur une valeur produite par ce module).
		const char *NkEmotionToString(NkEmotion e);

		// Parse un nom d'émotion (insensible à la casse). `ok` = false et
		// NK_EMOTION_NEUTRAL si le nom n'est reconnu par AUCUNE entrée de la table
		// (c'est ce test qui fait échouer le schéma sur une émotion halluciné par
		// un LLM, ex. "furieux" au lieu de "anger").
		NkEmotion NkEmotionFromString(const NkString &name, bool &ok);

		// =========================================================================
		// NkPersonalityTrait — un trait nommé, intensité normalisée [0,1]
		// (ex. {"bravoure", 0.8}, {"loyaute", 0.6}).
		// =========================================================================
		struct NkPersonalityTrait {
				NkString name;
				float32 value = 0.5f; // [0,1]
		};

		// =========================================================================
		// NkSceneObjective — ce que le personnage cherche à accomplir DANS la scène
		// courante (pas un objectif de vie général : un objectif de BEAT).
		// =========================================================================
		struct NkSceneObjective {
				NkString description;
				int32 priority = 0;	 // 0 = fond, plus haut = plus urgent/prioritaire
				float32 urgency = 0.f; // [0,1] — pression temporelle perçue
		};

		// =========================================================================
		// NkHistoryEvent — un événement court de l'historique de scène (texte +
		// horodatage). L'historique reste COURT par design (mémoire de travail du
		// beat courant, pas un journal complet) : voir `NkRoleContext::maxHistory`.
		// =========================================================================
		struct NkHistoryEvent {
				NkString text;
				float32 timestamp = 0.f;
		};

		// =========================================================================
		// NkRoleContext — contexte de rôle complet d'UN personnage pour UN beat.
		// =========================================================================
		class NkRoleContext {
			public:
				NkString roleName;						  // ex. "Garde du village"
				NkVector<NkPersonalityTrait> traits;		  // personnalité
				NkEmotion emotion = NkEmotion::NK_EMOTION_NEUTRAL;
				float32 emotionIntensity = 0.f;			  // [0,1]
				NkSceneObjective objective;
				NkVector<NkHistoryEvent> history;			  // historique COURT (borné)
				uint32 maxHistory = 8;						  // taille max de l'historique

				// Ajoute un événement ; si l'historique dépasse `maxHistory`, l'événement
				// le PLUS ANCIEN est retiré (FIFO — mémoire de travail glissante).
				void PushHistory(const NkString &text, float32 timeStamp);

				// ── Sérialisation (NKSerialization) ─────────────────────────────────
				// Archive <-> struct (fidèle, round-trip garanti sur des valeurs valides).
				NkArchive ToArchive() const;
				bool FromArchive(const NkArchive &arc, NkString *outError = nullptr);

				// JSON texte <-> struct (passe par ToArchive/FromArchive + NkJSONWriter/
				// NkJSONReader). C'est la forme que produirait/consommerait le futur pont
				// directeur (NkGPT / API externe, brique 2/5).
				NkString ToJSON(bool pretty = true) const;
				bool FromJSON(const NkString &json, NkString *outError = nullptr);

				// Auto-test headless (aucun GPU, aucun réseau) : construit un contexte,
				// vérifie round-trip archive ET JSON, vérifie la FIFO d'historique,
				// et vérifie que le SCHÉMA rejette plusieurs variantes malformées.
				static bool SelfTest();
		};

		// =========================================================================
		// NkRoleContextSchema — validation STRICTE d'une archive candidate AVANT
		// de la convertir en NkRoleContext. C'est la porte d'entrée anti-texte-libre
		// évoquée par le roadmap M4bis : toute sortie IA doit passer ici.
		// Champs requis + bornes :
		//   role                 : string, non vide
		//   emotion.primary      : string, doit matcher NkEmotionFromString
		//   emotion.intensity    : nombre, [0,1]
		//   objective.description: string, non vide
		//   objective.priority   : entier, >= 0
		//   objective.urgency    : nombre, [0,1]
		//   traits[]             : objets {name:string non vide, value:[0,1]}
		//   history[]            : objets {text:string, timestamp:nombre} — optionnel,
		//                          mais si présent, chaque élément DOIT être bien formé.
		// =========================================================================
		struct NkRoleContextSchema {
				// Retourne true si `arc` respecte le schéma ci-dessus. Sinon, false et
				// `outError` (si fourni) décrit la PREMIÈRE violation rencontrée.
				static bool Validate(const NkArchive &arc, NkString &outError);
		};

	} // namespace renderer
} // namespace nkentseu
