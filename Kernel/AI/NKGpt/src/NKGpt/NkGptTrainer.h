// =============================================================================
// NKGpt/NkGptTrainer.h — entraîneur GPT RÉUTILISABLE (config + boucle Fit + Generate)
// -----------------------------------------------------------------------------
// N'importe quelle application remplit un NkGptConfig (programmatique, PAS de variables
// d'environnement ici — c'est le rôle de l'app) puis :
//     NkGptTrainer t(cfg);
//     if (t.Prepare()) { t.Fit(); t.GenerateFinal(); }
// Encapsule : chargement corpus/checkpoint, BPE, construction modèle, accumulation de
// gradient, masquage de loss (instruction-tuning), LR schedule (warmup+cosine), checkpoint
// périodique, reprise d'entraînement, génération autoregressive. Namespace nkentseu::ai::gpt.
// AUTEUR : Rihen — LICENCE : Propriétaire - usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKGpt/NkGptCore.h"
#include "NKGpt/NkSampling.h"
#include "NKNN/NkNN.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKTensor/NkTensor.h"
#include "NKAutograd/NkVar.h"

namespace nkentseu {
	namespace ai {
		namespace gpt {

			// Config d'un entraînement (remplie par l'app ; défauts = comportement historique).
			struct NkGptConfig {
				public:
					// Corpus : soit un dossier (équilibré par langue), soit un fichier unique.
					NkString corpusDir;			// dossier *.txt (si corpusFile vide)
					NkString corpusFile;		// fichier unique (prioritaire s'il est non vide)
					nk_size maxChars = 1200000; // budget total de caractères
					int merges = 600;			// fusions BPE (ignoré si bpePath est fourni)

					// Tokenizer PRÉ-ENTRAÎNÉ (fichier « NKBP », cf NKData/NkBpeTrainer.h).
					// Au-delà de quelques centaines de fusions, entraîner le BPE à chaque
					// démarrage est absurde : il ne dépend que du corpus, pas du modèle, et
					// deux entraînements successifs doivent partager EXACTEMENT le même
					// découpage sous peine d'incompatibilité des poids d'embedding. On
					// l'entraîne donc une fois, on le range dans un fichier, et on le relit.
					// Non vide => `merges` est ignoré et le BPE n'est pas ré-entraîné.
					NkString bpePath;

					// Marqueur de début de réponse pour le masquage de loss
					// (instruction-tuning : la question ne compte pas dans la perte).
					// Défaut = comportement historique. Les corpus n'écrivent pas tous
					// l'accent : un marqueur qui ne correspond à rien désactive
					// silencieusement le masquage, d'où ce réglage explicite.
					NkString qaMarker = NkString("Réponse: ");

					// Modèle.
					int64 T = 128, d = 256, H = 8, L = 4, B = 16;

					// Entraînement.
					int steps = 300;
					int accum = 1;	   // micro-lots accumulés (batch effectif = B*accum)
					int warmup = -1;   // <0 -> steps/20 (5%)
					int saveEvery = 0; // checkpoint tous les N pas (0 = fin seule)
					float lr = 3e-4f;  // pic de learning-rate (puis cosine, plancher 10%)

					// Validation (held-out) : mesure la généralisation (≠ mémorisation).
					float valFrac = 0.f; // fraction de queue de CHAQUE langue réservée au val (0 = désactivé)
					int valEvery = 0;	 // évalue le val tous les N pas (0 = seulement à la fin si valFrac>0)

					// Checkpoint / reprise.
					NkString savePath;	 // sauvegarde (vide = pas de sauvegarde)
					NkString loadPath;	 // checkpoint à charger (vide = fresh)
					bool resume = false; // loadPath + resume = continuer l'entraînement

					// Génération.
					NkString seed = NkString("Le ");
					int genLen = 400;
					NkString genLang;	 // "" = auto (pas de tag)
					bool verbose = true; // impressions de progression
			};

			class NkGptTrainer {
				public:
					explicit NkGptTrainer(const NkGptConfig &cfg);
					~NkGptTrainer();
					NkGptTrainer(const NkGptTrainer &) = delete;
					NkGptTrainer &operator=(const NkGptTrainer &) = delete;

					// Charge corpus/checkpoint, entraîne le BPE (ou le reprend), construit le modèle et
					// charge les poids si un checkpoint est fourni. Retourne false en cas d'erreur.
					bool Prepare();

					// Boucle d'entraînement (accumulation, masquage, LR schedule, checkpoint périodique).
					void Fit();

					// Génération autoregressive depuis une amorce (langIdx>=0 => préfixe le tag de langue).
					// Utilise le KV-cache quand le contexte total tient dans la fenêtre T (rapide),
					// sinon repli fenêtre glissante. Échantillonnage température seule (compat).
					NkString Generate(const NkString &seed, int nToks, double temp, int langIdx);

					// Variante avec échantillonnage top-k / top-p (nucleus) — cf. NkSampling.h.
					NkString Generate(const NkString &seed, int nToks, const NkSampleParams &sp, int langIdx);

					// Génération finale : une sortie par langue si multilingue.
					void GenerateFinal();

					// Sauvegarde le checkpoint (dims + BPE + langues + poids).
					bool Save(const char *path);

					// Accès (après Prepare).
					bool IsReady() const {
						return mGpt != nullptr;
					}

					bool UseGpu() const {
						return mUseGpu;
					}

					double LastEma() const {
						return mEma;
					}

					int GenLangIndex() const {
						return mGenLang;
					}

					const NkVector<NkString> &Langs() const {
						return mLangs;
					}

					const Bpe &Tokenizer() const {
						return mBpe;
					}

				private:
					nk_size EncodeCorpus(const NkVector<NkString> &texts); // texts -> mLangData/mLangMask (masque QA)
					void MakeBatch(NkTensor &x, NkTensor &oneHot);		   // lot d'entraînement (depuis mLangData)
					// Fabrique un lot x[B,T] + cible one-hot masquée depuis une source (train ou val).
					void MakeBatchFrom(const NkVector<NkVector<float>> &data, const NkVector<NkVector<float>> &mask,
									   NkTensor &x, NkTensor &oneHot);
					// Idem mais cible = INDICES [B*T] (id par position ; -1 = masquée) au lieu du one-hot [B*T,V].
					// Économise le one-hot dense. Utilisé par l'entraînement (SoftmaxCrossEntropyIndexed).
					void MakeBatchIdx(NkTensor &x, NkTensor &targetIdx);
					void MakeBatchIdxFrom(const NkVector<NkVector<float>> &data, const NkVector<NkVector<float>> &mask,
										  NkTensor &x, NkTensor &targetIdx);
					double EvaluateVal(int nBatches); // perte moyenne sur le held-out (forward seul) ; -1 si pas de val
					double NextRand();				  // LCG déterministe [0,1)
					void FillMeta(GptMeta &meta) const; // dims + BPE + langues (pour la sauvegarde)

					NkGptConfig mCfg;
					bool mUseGpu = false;
					Bpe mBpe;
					NkVector<NkString> mLangs;
					NkVector<NkVector<float>> mLangData, mLangMask; // entraînement (par langue)
					NkVector<NkVector<float>> mValData, mValMask;	// held-out validation (queue de chaque langue)
					int mV = 0, mNByte = 0;
					int64 mT = 0, mD = 0, mH = 0, mL = 0, mB = 0;
					nn::NkGPT *mGpt = nullptr; // tas (dims connues après Prepare)
					NkVector<NkVar> mParams;
					int mGenLang = -1;
					uint64 mRng = 0x9E3779B97F4A7C15ull;
					double mEma = 0.0;
					// Perte du tout premier pas : sert de reference au filet de securite qui
					// detecte un entrainement qui ne calcule rien.
					double mPerteInitiale = 0.0;

					// État optimiseur repris d'un checkpoint (reprise parfaite du schedule).
					NkVector<NkTensor> mOptM, mOptV;
					int64 mResumeStep = 0;
					bool mHasOptState = false;
			};

		} // namespace gpt
	} // namespace ai
} // namespace nkentseu
