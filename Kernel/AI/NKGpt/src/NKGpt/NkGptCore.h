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

					// ---- ARCHITECTURE (v6, 2026-08-21) -------------------------------
					// POURQUOI CES DEUX BOOLEENS SONT DANS LE FICHIER.
					// Le checkpoint portait V, d, tetes, couches, T -- tout ce qui decrit
					// la FORME des tenseurs -- mais pas la NATURE du modele. Or NkGPT et
					// NkLlamaLM n'ont ni le meme nombre de tenseurs ni les memes formes :
					// un modele entraine avec `--llama --tying` puis recharge sans ces
					// drapeaux echoue sur « Poids du checkpoint incompatibles avec les
					// dims », sans jamais dire QUELLE architecture il aurait fallu
					// demander. Vecu le 2026-08-21 : 21 heures de calcul (91,4 M de
					// parametres) inutilisables en inference parce que `--parler` ne
					// lisait pas les deux drapeaux que `--train` avait lus.
					//
					// Une information necessaire pour relire un fichier appartient au
					// fichier, pas a la ligne de commande de celui qui le relit.
					bool architectureLlama = false;
					bool weightTying = false;
					// Vrai seulement si le FICHIER porte l'information (v6+). Un
					// checkpoint v3-v5 laisse ce champ a false : l'appelant sait alors que
					// les deux booleens ci-dessus ne veulent rien dire et qu'il doit s'en
					// remettre a ce que la ligne de commande demande. Sans ce troisieme
					// champ, « le fichier dit non » et « le fichier ne dit rien » seraient
					// indiscernables -- et tous les anciens checkpoints Llama se
					// rechargeraient en NkGPT, c'est-a-dire pas du tout.
					bool architectureConnue = false;
			};

			// Sauvegarde le checkpoint. Si `optM`/`optV` sont fournis (non nuls), écrit AUSSI l'état de
			// l'optimiseur Adam (moments 1er/2e + `step` = nombre total de pas effectués) => reprise
			// PARFAITE du schedule (pas de warmup ni de pic de perte à la reprise). Format « NKGP » v4 :
			// v3 + bloc optionnel {hasOpt, step, moments}. Les lecteurs v3 restent compatibles (ils
			// s'arrêtent après les poids). optM/optV nuls => hasOpt=0 (fichier v4 sans état optimiseur).
			// v5 (2026-08-17) : + queue {rng} = etat du flux aleatoire d'echantillonnage, pour qu'une
			// reprise ne re-tire pas les memes fenetres. Rotation a TROIS exemplaires (<path>, .prev,
			// .prev2), ecriture forcee sur disque (_commit) avant renommage.
			// v6 (2026-08-21) : + 2 octets d'ARCHITECTURE dans l'entete, juste apres les langues
			// {architectureLlama, weightTying}. LECTURE RETRO-COMPATIBLE : tous les lecteurs
			// branchent sur `ver`, donc un fichier v3/v4/v5 deja ecrit continue de se charger
			// exactement comme avant -- verifie sur un checkpoint v5 de 844 Mo. En revanche un
			// fichier v6 n'est PAS lisible par un binaire anterieur (il refuse ver > 5) : les exes
			// isoles des campagnes en cours doivent etre rafraichis avant d'ecrire du v6.
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
