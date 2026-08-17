// =============================================================================
// NKGpt/NkGptCore.h — briques RÉUTILISABLES d'un GPT from-scratch (NKAI)
// -----------------------------------------------------------------------------
// Extrait de l'app NKGptTrain pour être utilisable par N'IMPORTE QUELLE application :
//   - Tokenizer BPE from-scratch (Bpe, TrainBpe) — GÉNÉRALISÉ dans NKData
//     (`data::NkBpe`/`data::TrainBpe`, NKData/NkTokenizer.h) depuis 2026-07-26 :
//     aucune logique ici n'était spécifique GPT, seuls des ALIAS restent pour ne
//     pas casser le code existant (`gpt::Bpe` == `data::NkBpe`).
//   - Chargement de corpus (fichier / dossier équilibré par langue) — reste ici,
//     spécifique (entête Project Gutenberg, tags de langue par nom de fichier).
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
#include "NKData/NkTokenizer.h"

namespace nkentseu {
	namespace ai {
		namespace gpt {

			// ---- Tokenizer BPE : alias vers la version générique de NKData -------------
			// (déplacée là car aucune logique GPT-spécifique ; cf NKData/NkTokenizer.h).
			using NkMerge = data::NkMerge;
			using I64Map = data::NkI64Map;
			using Bpe = data::NkBpe;
			using data::TrainBpe;
			using data::DecodeAll;

			// ---- Corpus ----------------------------------------------------------------
			// Lit un fichier, saute l'entête Project Gutenberg si présent, cape à maxChars.
			NkString LoadCorpus(const char *path, nk_size maxChars);
			// Langue d'un chemin = préfixe (avant le premier '_') du nom de fichier (1-4 car.).
			NkString LangOf(const NkString &path);
			// Charge tout un dossier *.txt GROUPÉ PAR LANGUE : remplit `langs` + `texts`
			// (parallèles), chaque langue ~totalCap/nbLangues caractères (équilibrage).
			void LoadCorpusByLang(const NkString &dir, nk_size totalCap, NkVector<NkString> &langs,
								  NkVector<NkString> &texts);

			// ---- Checkpoint « NKGP » v3 : dims + BPE (fusions) + langues + poids (CPU) --
			struct GptMeta {
					int32 V = 0, d = 0, H = 0, L = 0, T = 0;
					NkVector<NkMerge> merges;
					NkVector<NkString> langs;
			};

			// Sauvegarde le checkpoint. Si `optM`/`optV` sont fournis (non nuls), écrit AUSSI l'état de
			// l'optimiseur Adam (moments 1er/2e + `step` = nombre total de pas effectués) => reprise
			// PARFAITE du schedule (pas de warmup ni de pic de perte à la reprise). Format « NKGP » v4 :
			// v3 + bloc optionnel {hasOpt, step, moments}. Les lecteurs v3 restent compatibles (ils
			// s'arrêtent après les poids). optM/optV nuls => hasOpt=0 (fichier v4 sans état optimiseur).
			// v5 (2026-08-17) : + queue {rng} = etat du flux aleatoire d'echantillonnage, pour qu'une
			// reprise ne re-tire pas les memes fenetres. Rotation a TROIS exemplaires (<path>, .prev,
			// .prev2), ecriture forcee sur disque (_commit) avant renommage.
			bool SaveCheckpoint(const char *path, const GptMeta &m, const NkVector<NkVar> &params,
								const NkVector<NkTensor> *optM = nullptr, const NkVector<NkTensor> *optV = nullptr,
								int64 step = 0, uint64 rng = 0);

			// Lit le fichier JUSQU'AU BOUT (entete, poids, etat optimiseur, queue) sans rien charger :
			// vrai si le checkpoint est complet et coherent. `stepOut` recoit le pas global (0 si v3).
			bool VerifierCheckpoint(const char *path, int64 *stepOut = nullptr);
			// Le plus recent VALIDE parmi <path>, <path>.prev, <path>.prev2 ; journalise le repli.
			bool ChoisirCheckpointValide(const char *path, NkString &retenu, int64 *stepOut = nullptr);

			bool LoadCheckpointMeta(const char *path, GptMeta &m);
			bool LoadCheckpointWeights(const char *path, NkVector<NkVar> &params);

			// Charge l'état optimiseur du checkpoint (moments + pas). Renvoie false si le fichier est
			// antérieur (v3) ou ne contient pas d'état (hasOpt=0). Moments rendus sur CPU.
			// `rng` (facultatif) recoit l'etat du flux aleatoire si le fichier est v5 ; sinon inchange.
			bool LoadCheckpointOptState(const char *path, NkVector<NkTensor> &optM, NkVector<NkTensor> &optV,
										int64 &step, uint64 *rng = nullptr);

		} // namespace gpt
	} // namespace ai
} // namespace nkentseu
