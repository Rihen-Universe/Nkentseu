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
					// Horizon ABSOLU (pas global visé). > 0 : le nombre de pas à faire
					// est calculé par le trainer lui-même (horizon − pas déjà effectués)
					// et `steps` est ignoré.
					//
					// POURQUOI : un nombre de pas RELATIF n'est pas robuste aux reprises.
					// Après une coupure de courant, un relanceur qui doit deviner combien
					// de pas restent se trompe dès qu'il lit une source incomplète — un
					// journal, par exemple, perd sa fin quand l'alimentation est coupée
					// (vécu le 2026-08-12 : relance calculée sur « pas 1200 » alors que le
					// checkpoint était à 5000 ; l'horizon a dérivé de 6000 à 9800, donc le
					// calendrier du pas d'apprentissage aussi). Le seul qui connaisse le
					// vrai pas global, c'est le CHECKPOINT — donc le trainer. Une cible
					// absolue rend la reprise IDEMPOTENTE : relancer dix fois avec le même
					// horizon donne le même entraînement.
					int64 horizon = 0;
					// Échantillons de texte affichés PENDANT l'entraînement, tous les
					// N pas (0 = jamais). C'est du CONFORT : ils montrent l'évolution,
					// ils ne calculent rien.
					//
					// ⚠️ Et ce confort peut TUER un run. La génération alloue en plus
					// de l'entraînement (contexte, cache d'attention) : sur une carte
					// déjà chargée par le bureau, ce pic supplémentaire déclenche un
					// VK_ERROR_OUT_OF_DEVICE_MEMORY qui emporte tout. Mesuré le
					// 2026-08-12 : run mort au pas 500 sur 6000, après quatre
					// générations réussies — quinze heures perdues pour cinq lignes
					// d'affichage. Sur un run long, mettre 0.
					int sampleEvery = 100;
					int accum = 1;	   // micro-lots accumulés (batch effectif = B*accum)
					int warmup = -1;   // <0 -> steps/20 (5%)
					int saveEvery = 0; // checkpoint tous les N pas (0 = fin seule)
					float lr = 3e-4f;  // pic de learning-rate (puis cosine, plancher 10%)

					// Validation (held-out) : mesure la généralisation (≠ mémorisation).
					float valFrac = 0.f; // fraction de queue de CHAQUE langue réservée au val (0 = désactivé)
					int valEvery = 0;	 // évalue le val tous les N pas (0 = seulement à la fin si valFrac>0)
					// ⚠️ MODE D'ECHANTILLONNAGE DES FENETRES — DECLARE, plus deduit.
					//
					//    0 = jamais de debut de bloc — decalage uniforme pur  [DEFAUT]
					//    1 = une fenetre sur deux demarre sur un debut de bloc
					//   -1 = auto (deduit du contenu) — RESERVE au rejeu des anciennes
					//        courses, JAMAIS le defaut : ce mode est precisement l'accident
					//        qu'on corrige. Un defaut a -1 aurait rendu le regime declarable
					//        tout en continuant a le subir — un choix apparent qui reporte la
					//        decision indefiniment.
					//
					// DEFAUT = 0 (uniforme pur), et c'est un choix, pas une reconduction :
					//  - c'est le regime sous lequel TOUTES les courses de reference ont
					//    tourne (`fr_course.txt` ne porte pas le marqueur) ;
					//  - le mode 1 a ete introduit pour que les exemples question/reponse
					//    soient vus ENTIERS. Sur le corpus complet, `mLangStarts` recense
					//    3 923 853 blocs dont 3 270 seulement sont des paires : demarrer sur
					//    une frontiere de paragraphe Wikipedia n'a rien a voir avec la raison
					//    d'origine, et cet effet-la n'a jamais ete mesure.
					//
					// POURQUOI CE PARAMETRE EXISTE. Le regime etait deduit d'un
					// `txt.Find("Reponse: ")` sur le fichier ENTIER : un corpus de 1,62 Go
					// basculait parce qu'un marqueur figurait dans ses 500 premiers
					// kilo-octets. Cela n'a JAMAIS ete decide, et sortir un fragment du
					// corpus suffisait a changer le regime d'entrainement de tout le reste
					// — en silence, comme effet de bord d'une operation de fichiers.
					//
					// Mesure du 14 aout 2026 : `fr_course.txt` ne contient pas le marqueur
					// (0 occurrence) tandis que `fr_ilyana.txt` en compte 4 950. Toutes les
					// courses de reference tournaient donc en uniforme pur, et le corpus
					// complet basculera au grand run.
					int debutsDeBloc = 0;

					// Nombre de fenetres du jeu de validation FIGE (0 = valeur par defaut, 192).
					// Reglable parce que la precision de la mesure en depend directement :
					// trop peu de fenetres et l'ecart entre deux entrainements se lit dans le
					// tirage plutot que dans les modeles.
					int valFenetres = 0;

					// Checkpoint / reprise.
					NkString savePath;	 // sauvegarde (vide = pas de sauvegarde)
					NkString loadPath;	 // checkpoint à charger (vide = fresh)
					bool resume = false; // loadPath + resume = continuer l'entraînement

					// NOUVELLE PHASE : repartir des poids, mais avec un calendrier
					// d'apprentissage NEUF (warmup + pic demandé), au lieu de
					// prolonger celui du run précédent.
					//
					// POURQUOI CETTE DISTINCTION EXISTE. Reprendre un entraînement
					// interrompu et commencer une phase sur un AUTRE corpus sont deux
					// gestes opposés, que `resume` confondait. Le premier veut la
					// continuité — reprendre le calendrier là où il s'est arrêté est
					// exactement ce qu'il faut. Le second veut apprendre du neuf, et
					// hérite alors d'un pas d'apprentissage déjà décru au plancher.
					//
					// Mesuré le 2026-08-10 : une phase destinée à enseigner la
					// citation a tourné 900 pas à lr = 1e-05, n'a RIEN appris du
					// geste visé, et a fait BAISSER la batterie de contrôle de 8/19 à
					// 6/19. Assez pour graver un nom repete des milliers de fois, très
					// insuffisant pour un comportement nouveau.
					//
					// Les moments d'Adam sont CONSERVES : ils décrivent la courbure
					// vue jusque-là, qui reste valable, et les jeter provoquerait un
					// à-coup de perte au redémarrage.
					bool freshSchedule = false;

					// Architecture du modèle. false = NkGPT (LayerNorm, positions
					// apprises, GELU) ; true = NkLlamaLM (RMSNorm, RoPE, SwiGLU).
					//
					// ⚠️ RoPE ne grave AUCUNE longueur maximale dans les poids : la
					// position vit dans une rotation, pas dans une table apprise. Une
					// fenêtre T=256 en positions apprises est définitive — l'élargir
					// imposerait de réentraîner le modèle entier.
					//
					// ⚠️ NkLlamaLM n'a pas encore de décodage incrémental : la
					// génération y retraite tout le contexte à chaque token. Utilisable
					// pour l'ENTRAÎNEMENT, coûteux pour de longues générations.
					bool architectureLlama = false;

					// Graine d'initialisation des poids. Exposée parce qu'un écart
					// entre deux graines de la MÊME architecture donne le plancher de
					// bruit : sans lui, on ne sait pas si un écart entre architectures
					// veut dire quelque chose.
					uint32 seedPoids = 1234u;
					// Lie la projection de sortie a la table d'embedding (NkLlamaLM seul).
					// Economise vocab x d parametres et change la NATURE du gradient recu
					// par la table : creux par la recherche, dense par la projection.
					bool weightTying = false;

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
					// ⚠️ POIDS SEULS — SANS l'état de l'optimiseur. À n'utiliser que pour
					// exporter un modèle destiné à la seule génération. NE PAS l'appeler
					// après `Fit()` : celui-ci a déjà écrit un checkpoint COMPLET (moments
					// d'Adam + pas global) permettant une reprise exacte, et ce Save-ci
					// l'écraserait par une version dégradée — la reprise repartirait alors
					// sans état d'optimiseur, avec un pic de perte.
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
					double EvaluateVal(); // perte moyenne sur le held-out fige (forward seul) ; -1 si pas de val
					double NextRand();				  // LCG déterministe [0,1)
					void FillMeta(GptMeta &meta) const; // dims + BPE + langues (pour la sauvegarde)

					NkGptConfig mCfg;
					bool mUseGpu = false;
					Bpe mBpe;
					NkVector<NkString> mLangs;
					NkVector<NkVector<float>> mLangData, mLangMask; // entraînement (par langue)

					// Position de DÉBUT de chaque bloc dans le flux de tokens.
					//
					// POURQUOI CETTE LISTE EXISTE. Le lot était prélevé à un décalage
					// tiré au hasard dans un flux plat. Pour de la prose, c'est
					// exactement ce qu'il faut. Mais un exemple STRUCTURÉ
					// (« Contexte: … Question: … Reponse: … ») fait environ 180 tokens
					// pour une fenêtre de 256 : tiré au hasard, il n'est entier qu'une
					// fois sur trois — et surtout, quand la fenêtre commence au milieu
					// du contexte, le modèle voit une question suivie de « Reponse: »
					// avec un contexte AMPUTÉ, et on lui apprend à produire une phrase
					// qui n'y figure pas. Autrement dit, on lui enseigne à INVENTER,
					// précisément ce qu'on cherchait à combattre.
					//
					// Mesuré : deux entraînements de 900 pas destinés à enseigner la
					// citation n'ont rien appris du geste, et ont fait baisser la
					// batterie de contrôle (8/19 -> 6/19, puis 5/19).
					NkVector<NkVector<int64>> mLangStarts;

					// Choisit où démarre une fenêtre : une fois sur deux au début d'un
					// bloc, une fois sur deux au hasard. Écrit UNE SEULE FOIS parce que
					// les deux fabriques de lot en ont besoin, et que deux copies du
					// même calcul finissent toujours par diverger.
					int64 ChoisirDecalage(int li, int64 N);
					NkVector<NkVector<float>> mValData, mValMask;	// held-out validation (queue de chaque langue)

					// Fenetres de validation FIGEES : (langue, decalage), construites une
					// seule fois dans Prepare, relues telles quelles a chaque evaluation.
					//
					// POURQUOI. Les fenetres etaient tirees au hasard a CHAQUE appel de
					// EvaluateVal. Deux consequences :
					//  - la perte du pas 50 et celle du pas 300 ne portaient pas sur le meme
					//    texte, donc leur difference melange le progres du modele et la
					//    difficulte inegale des extraits ;
					//  - le tirage consommait le meme flux aleatoire que les lots
					//    d'entrainement, ce qui liait deux choses sans aucune raison de l'etre.
					// Un jeu fige supprime les deux. Le tirage utilise un flux SEPARE et un
					// germe constant : la validation ne bouge donc pas quand --graine change,
					// et deux entrainements comparent bien leurs pertes sur le MEME texte.
					struct FenetreVal {
							int langue = 0;
							int64 decalage = 0;
					};
					NkVector<FenetreVal> mValFenetres;
					void ConstruireFenetresVal(); // une seule fois, apres le decoupage held-out
					// Lot de validation numero `i` du jeu fige (fenetres [i*B, i*B+B)).
					void MakeBatchValFige(int indexLot, NkTensor &x, NkTensor &targetIdx);

					int mV = 0, mNByte = 0;
					int64 mT = 0, mD = 0, mH = 0, mL = 0, mB = 0;
					nn::NkGPT *mGpt = nullptr; // tas (dims connues après Prepare)
					// Architecture ALTERNATIVE : RMSNorm + RoPE + SwiGLU.
					//
					// Deux pointeurs plutôt qu'une interface commune, et c'est
					// délibéré : les deux classes exposent bien `Forward` et
					// `Parameters` avec les mêmes signatures, mais NkLlamaLM n'a PAS
					// de `ForwardStep` (décodage incrémental avec cache clé/valeur).
					// Une interface commune mentirait sur ce que le modèle sait faire,
					// et le manque n'apparaîtrait qu'à l'exécution.
					// Exactement UN des deux pointeurs est non nul.
					nn::NkLlamaLM *mLlama = nullptr;
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
