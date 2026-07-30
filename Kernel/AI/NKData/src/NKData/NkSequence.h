// =============================================================================
// NKData/NkSequence.h — vocabulaire, séquences, padding (NKAI, Jalon 2 : texte).
// -----------------------------------------------------------------------------
// Complète le tokenizer BPE (NkTokenizer.h) avec deux briques génériques :
//   - `NkVocab` : vocabulaire MOT-À-MOT (whitespace), distinct du BPE octet-à-octet
//     — utile pour des tâches simples (bag-of-words, RNN mot-niveau) sans entraîner
//     de BPE. Réservé : id 0 = <pad>, id 1 = <unk>.
//   - `PadSequences` : empaquette des séquences d'IDENTIFIANTS de longueur VARIABLE
//     (sortie de `NkVocab::Encode` OU de `data::NkBpe::Encode`, les deux produisent
//     des `NkVector<int32>`) en un batch RECTANGULAIRE `[B, Tmax]` complété par
//     padding + masque — la brique manquante pour entraîner un modèle séquentiel
//     (RNN/Transformer) avec NKTrain/NKNN sur des lots réels.
// Convention (alignée sur NkGptTrainer::MakeBatch) : les identifiants sont stockés
// en F32 dans le NkTensor (permet de les brancher directement sur
// `autograd::Embedding`, qui attend déjà des indices en tenseur flottant).
// Namespace : nkentseu::ai::data. Zéro STL (NkHashMap/NkVector/NkString).
// =============================================================================
#pragma once

#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Associative/NkHashMap.h"
#include "NKTensor/NkTensor.h"

namespace nkentseu {
	namespace ai {
		namespace data {

			// -----------------------------------------------------------------
			// Vocabulaire mot-à-mot : texte <-> identifiants entiers.
			// -----------------------------------------------------------------
			class NkVocab {
				public:
					static const int32 kPadId = 0; // "<pad>"
					static const int32 kUnkId = 1; // "<unk>"

					NkVocab();

					// Construit le vocabulaire depuis des textes (découpage sur espace/'\n'/'\t'/'\r',
					// comme `NkBpe::PreTok` mais mot ENTIER, pas octet). Ne garde que les mots dont la
					// fréquence totale >= minFreq (les autres restent <unk> à l'encodage) — trie par
					// fréquence décroissante pour un vocabulaire compact et déterministe.
					void BuildFromTexts(const NkVector<NkString> &texts, int32 minFreq = 1);

					// Identifiant d'un mot (kUnkId si absent du vocabulaire).
					int32 IdOf(const NkString &word) const;

					// Mot d'un identifiant ("<unk>" si hors bornes).
					const NkString &WordOf(int32 id) const;

					uint32 Size() const {
						return (uint32)mIdToWord.Size();
					}

					// Découpe `text` en mots (espaces) puis IdOf() par mot -> identifiants.
					void Encode(const NkString &text, NkVector<int32> &outIds) const;

					// Reconstruit un texte (mots séparés par un espace) depuis des identifiants.
					NkString DecodeAll(const NkVector<int32> &ids) const;

				private:
					NkHashMap<NkString, int32> mWordToId;
					NkVector<NkString> mIdToWord;
			};

			// -----------------------------------------------------------------
			// Batch de séquences rectangulaire, prêt pour l'entraînement.
			// -----------------------------------------------------------------
			struct NkSeqBatch {
					NkTensor ids;			// [B, Tmax] F32 (identifiants, padId au-delà de chaque longueur)
					NkTensor mask;			// [B, Tmax] F32 (1.0 = token réel, 0.0 = padding)
					NkVector<int32> lengths; // [B] longueur RÉELLE (avant padding) de chaque séquence
			};

			// Empaquette `seqs` (longueurs variables) en un `NkSeqBatch` rectangulaire.
			// `maxLen <= 0` => auto-détecté (longueur de la plus longue séquence du lot).
			// Les séquences plus longues que `maxLen` sont TRONQUÉES (garde le préfixe).
			NkSeqBatch PadSequences(const NkVector<NkVector<int32>> &seqs, int32 padId = 0, int32 maxLen = -1);

		} // namespace data
	} // namespace ai
} // namespace nkentseu
