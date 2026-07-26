// =============================================================================
// NKAgent/NkAgentLLMReasoning.h — pont NkAgent -> NKInfer (raisonnement par LLM
// pour une décision AMBIGUË) (NKAI, Phase 4, Jalon 4b).
//
// SCÉNARIO CIBLE : la politique tabulaire (rl::NkQLearning, cf NkAgentPolicy)
// n'a PAS encore appris (table Q neuve, toutes les valeurs à 0 pour l'état
// courant) — l'espace d'action est alors « ambigu » au sens où aucune action
// n'est objectivement meilleure qu'une autre du point de vue de la politique
// apprise seule (contrairement aux Jalons 1-3, où Q distingue déjà les
// actions une fois entraînée). Ce module câble une décision de repli RÉELLE
// via NKInfer : forward pass Qwen2.5 authentique, poids GGUF réels
// (déjà livrés et prouvés — NKInfer/ROADMAP.md Jalon 3, NkQwen2LayerForward/
// NkKVCache/NkGGUFDequant), PAS un mock.
//
// LIMITE ASSUMÉE ET DOCUMENTÉE (cf NKInfer/ROADMAP.md) : aucun encodeur BPE
// n'existe dans ce dépôt. Le « prompt » ne peut donc PAS être du texte libre.
// Choix : encoder (état brut, état-but) en UNE SÉQUENCE DE VRAIS TOKENS du
// vocabulaire du modèle — les tokens-CHIFFRES ('0'..'9', cherchés par match
// exact dans tokenizer.ggml.tokens, relu en entier via
// NkGGUFReadFullStringArray, même fonction déjà utilisée par NKLLMInferTest)
// représentant les chiffres décimaux de l'état et du but, précédés du token
// BOS réel. La « décision » est lue en restreignant les logits de sortie du
// forward AUX 4 TOKENS-CHIFFRES '0'..'3' (qui correspondent EXACTEMENT aux 4
// actions de rl::NkGridWorld/NkKeyDoorGridWorld : 0=haut 1=bas 2=gauche
// 3=droite) et en prenant l'argmax parmi ces 4 candidats — pas de génération
// libre, pas de décodage de texte nécessaire. C'est une preuve de CÂBLAGE
// bout-en-bout (état agent -> tokens réels -> forward Qwen2 réel, poids réels
// déquantifiés à la demande -> logits réels -> action), PAS une preuve de
// qualité de raisonnement linguistique (le modèle ne « comprend » pas
// littéralement une grille — hors de portée sans tokenizer BPE, explicitement
// hors scope de ce jalon, cf mission).
//
// COÛT / LATENCE (mesuré, cf NKInfer/ROADMAP.md Jalon 3, NKLLMInferTest) :
// ~2.9 s/couche réelle en build Debug (déquantification Q4_K/Q6_K + forward),
// soit ~80-90 s pour un forward complet 28 couches (Qwen2.5 7B Instruct) — le
// coût des logits est réduit au minimum : SEULES les 4 lignes candidates du
// tenseur de sortie sont déquantifiées (cf DequantSelectiveRows dans le .cpp),
// pas les 152064 lignes complètes de lm_head (optimisation par rapport à
// NKLLMInferTest, qui déquantifiait lm_head en entier ~6s). CE N'EST PAS
// ADAPTÉ AU TEMPS RÉEL dans l'état actuel — une décision « urgente » doit
// continuer à utiliser la politique tabulaire (Step()/StepWithGoals) ; ce
// pont est réservé à un TRÈS PETIT nombre de décisions hors-ligne / de repli
// (cf mission — NKAgentLLMTest n'en déclenche que 2-3 RÉELLES).
//
// Zéro STL : NkVector/NkString/NkFile (NKFileSystem) uniquement, comme le
// reste de NKInfer. N'ajoute AUCUNE dépendance à Step()/StepWithGoals/
// StepWithPersonality existants (module additif, appelé explicitement par
// l'appelant quand IL juge la décision ambiguë — pas d'appel automatique
// caché dans la boucle de décision, vu le coût).
// Namespace : nkentseu::ai::agent.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKInfer/NkGGUFLoader.h"
#include "NKInfer/NkQwen2Block.h"
#include "NKInfer/NkKVCache.h"

namespace nkentseu {
	namespace ai {
		namespace agent {

			// Modèle Qwen2 référencé sur disque (métadonnées + vocabulaire déjà lus ;
			// AUCUN poids de couche n'est gardé déquantifié en RAM entre deux
			// décisions — chaque appel à NkAgentDecideViaLLM redéquantifie les
			// couches nécessaires depuis `path`, cf coût documenté ci-dessus).
			struct NkAgentLLMModel {
					NkString path;
					infer::NkGGUFFile gguf;
					infer::NkQwen2Config cfg;
					uint64 bosId = 0;
					uint64 blockCount = 0; // nb de couches RÉELLES du modèle (métadonnée)
					const infer::NkGGUFTensorInfo *embTensor = nullptr;	// pointe dans gguf.tensors (durée de vie = model)
					const infer::NkGGUFTensorInfo *lmHeadTensor = nullptr; // idem ; nullptr si "tied" (= embTensor)
					NkVector<NkString> vocab;								// vocabulaire complet (tokenizer.ggml.tokens)
					int32 digitTokenId[10];								// tok('0')..tok('9'), -1 si introuvable
					bool ready = false;
			};

			// Charge les métadonnées + le vocabulaire complet + retrouve les 10
			// tokens-chiffres depuis un blob GGUF réel (`ggufPath`) — ne déquantifie
			// AUCUN poids de couche (cf NkAgentDecideViaLLM, à la demande). false si
			// le fichier n'est pas un GGUF Qwen2 valide, ou si un chiffre '0'..'9' est
			// introuvable dans le vocabulaire (message dans `outError`).
			bool NkAgentLoadLLMModel(const char *ggufPath, NkAgentLLMModel &outModel, NkString *outError = nullptr);

			// Résultat d'UNE décision par raisonnement LLM.
			struct NkAgentLLMDecision {
					bool ok = false;
					int32 action = -1;		   // 0..3 (cf rl::NkGridWorld), -1 si échec
					float candidateLogits[4]; // logits réels des tokens '0'..'3' (avant argmax)
					uint32 nLayers = 0;		   // nb de couches RÉELLEMENT exécutées pour cette décision
					double seconds = 0.0;	   // temps mesuré (NkChrono), réel
			};

			// Décision de repli RÉELLE par LLM pour un état jugé « ambigu » par la
			// politique tabulaire (cf en-tête de fichier pour le scénario complet et
			// les limites). `nLayers` : nombre de couches RÉELLES du modèle à
			// exécuter (borné à model.blockCount ; mission : réduire ce nombre est un
			// choix EXPLICITE si le budget temps ne permet pas 28 couches complètes
			// — cf documentation d'appel). Forward pass, KV-cache RÉELS (NKInfer,
			// aucun mock) ; SEULE la restriction aux 4 tokens-chiffres candidats et
			// l'encodage numérique du prompt sont spécifiques à ce pont (NKAgent),
			// pas à NKInfer (module inchangé).
			NkAgentLLMDecision NkAgentDecideViaLLM(const NkAgentLLMModel &model, uint32 rawState, uint32 targetState,
													uint32 nLayers);

		} // namespace agent
	} // namespace ai
} // namespace nkentseu
