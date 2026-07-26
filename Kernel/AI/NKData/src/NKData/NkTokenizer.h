// =============================================================================
// NKData/NkTokenizer.h — tokenizer texte GÉNÉRIQUE from-scratch (NKAI, Jalon 2).
// -----------------------------------------------------------------------------
// Extrait/généralisé depuis `NKGpt/NkGptCore.h` (Byte-Pair Encoding, jusque-là
// spécifique à l'entraîneur GPT) pour être utilisable par N'IMPORTE QUEL module
// (pas seulement NkGptTrainer) : texte -> identifiants (Encode) et identifiants
// -> texte (Decode/DecodeAll). Rien ici n'est spécifique GPT (pas de corpus
// Gutenberg, pas de checkpoint modèle) : ça reste dans NKGpt qui réexporte ces
// types via des alias (`gpt::Bpe` == `data::NkBpe`) pour rester compatible.
// Namespace : nkentseu::ai::data. Zéro STL, zéro allocation brute (NkVector).
// =============================================================================
#pragma once

#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {
		namespace data {

			// ---- Table de hachage int64->int64 (open addressing, zéro-STL) -------------
			// Sert au comptage de paires BPE (Add=increment, argmax O(1)) et au rang des
			// fusions (Get). Capacité = puissance de 2 (Init).
			struct NkI64Map {
					NkVector<int64> keys;
					NkVector<int64> vals;
					int64 mask = 0;
					int64 bestKey = -1, bestVal = 0;
					void Init(int64 pow2);
					void Reset();
					static uint64 Hash(int64 k);
					void Add(int64 k, int64 w);
					int64 Get(int64 k, int64 def) const;
			};

			// ---- BPE (Byte-Pair Encoding) from-scratch, texte -> identifiants ----------
			struct NkMerge {
					int32 a = 0, b = 0;
			};

			struct NkBpe {
					NkVector<NkMerge> merges;
					NkVector<NkString> vocab; // id -> octets (décodage)
					NkI64Map rank;			  // (a,b) -> priorité de fusion

					// Taille du vocabulaire courant (256 octets + fusions apprises).
					int Base() const {
						return 256 + (int)merges.Size();
					}

					// (Re)construit `vocab` + `rank` depuis `merges` (après TrainBpe ou après
					// avoir chargé `merges` depuis un checkpoint).
					void BuildVocabRank();

					// Pré-tokenisation : découpe sur les espaces/'\n'/'\t'/'\r' (l'espace/le
					// séparateur reste collé au DÉBUT du mot suivant, convention GPT-2-like).
					static void PreTok(const NkString &text, NkVector<NkString> &words);

					// Encode un seul "mot" (segment issu de PreTok) en appliquant les fusions
					// apprises dans l'ordre de priorité (greedy, comme le BPE original).
					void EncodeWord(const NkString &w, NkVector<int32> &out) const;

					// Encode un texte complet (PreTok puis EncodeWord par mot) -> identifiants.
					void Encode(const NkString &text, NkVector<int32> &out) const;

					// Décode UN identifiant -> ses octets bruts.
					const NkString &Decode(int id) const {
						return vocab[(nk_size)id];
					}
			};

			// Entraîne le BPE : fusionne itérativement la paire adjacente la plus
			// fréquente sur `texts`, jusqu'à `nMerges` fusions (arrêt anticipé si plus
			// aucune paire ne se répète). Remplit `bpe.merges` + vocab/rank.
			void TrainBpe(const NkVector<NkString> &texts, int nMerges, NkBpe &bpe);

			// Détokenizer : reconstruit le texte complet depuis une séquence d'identifiants
			// (concatène Decode(id) pour chaque id, sans séparateur — les espaces sont déjà
			// des tokens grâce à la convention PreTok ci-dessus). Identifiants hors bornes
			// ignorés (robustesse face à un id corrompu/mal formé).
			NkString DecodeAll(const NkBpe &bpe, const NkVector<int32> &ids);

		} // namespace data
	} // namespace ai
} // namespace nkentseu
