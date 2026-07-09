// =============================================================================
// NKGpt/NkGptCore.h — briques RÉUTILISABLES d'un GPT from-scratch (NKAI)
// -----------------------------------------------------------------------------
// Extrait de l'app NKGptTrain pour être utilisable par N'IMPORTE QUELLE application :
//   - Tokenizer BPE from-scratch (Bpe, TrainBpe)
//   - Chargement de corpus (fichier / dossier équilibré par langue)
//   - Checkpoint « NKGP » v3 (dims + BPE + langues + poids), Save/Load
// Le pilotage haut niveau (config + boucle d'entraînement + génération) vit dans
// NkGptTrainer (étape suivante). Namespace : nkentseu::ai::gpt.
// AUTEUR : Rihen — LICENCE : Propriétaire - usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"

namespace nkentseu {
	namespace ai {
		namespace gpt {

			// ---- Corpus ----------------------------------------------------------------
			// Lit un fichier, saute l'entête Project Gutenberg si présent, cape à maxChars.
			NkString LoadCorpus(const char *path, nk_size maxChars);
			// Langue d'un chemin = préfixe (avant le premier '_') du nom de fichier (1-4 car.).
			NkString LangOf(const NkString &path);
			// Charge tout un dossier *.txt GROUPÉ PAR LANGUE : remplit `langs` + `texts`
			// (parallèles), chaque langue ~totalCap/nbLangues caractères (équilibrage).
			void LoadCorpusByLang(const NkString &dir, nk_size totalCap, NkVector<NkString> &langs,
								  NkVector<NkString> &texts);

			// ---- Table de hachage int64->int64 (open addressing, zéro-STL) -------------
			// Sert au comptage de paires BPE (Add=increment, argmax O(1)) et au rang des fusions (Get).
			struct I64Map {
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

			// ---- BPE (Byte-Pair Encoding) from-scratch ---------------------------------
			struct NkMerge {
					int32 a = 0, b = 0;
			};

			struct Bpe {
					NkVector<NkMerge> merges;
					NkVector<NkString> vocab; // id -> octets (décodage)
					I64Map rank;			  // (a,b) -> priorité de fusion

					int Base() const {
						return 256 + (int)merges.Size();
					}

					void BuildVocabRank();
					static void PreTok(const NkString &text, NkVector<NkString> &words);
					void EncodeWord(const NkString &w, NkVector<int32> &out) const;
					void Encode(const NkString &text, NkVector<int32> &out) const;

					const NkString &Decode(int id) const {
						return vocab[(nk_size)id];
					}
			};

			// Entraîne le BPE : fusionne itérativement la paire adjacente la plus fréquente.
			void TrainBpe(const NkVector<NkString> &texts, int nMerges, Bpe &bpe);

			// ---- Checkpoint « NKGP » v3 : dims + BPE (fusions) + langues + poids (CPU) --
			struct GptMeta {
					int32 V = 0, d = 0, H = 0, L = 0, T = 0;
					NkVector<NkMerge> merges;
					NkVector<NkString> langs;
			};

			bool SaveCheckpoint(const char *path, const GptMeta &m, const NkVector<NkVar> &params);
			bool LoadCheckpointMeta(const char *path, GptMeta &m);
			bool LoadCheckpointWeights(const char *path, NkVector<NkVar> &params);

		} // namespace gpt
	} // namespace ai
} // namespace nkentseu
