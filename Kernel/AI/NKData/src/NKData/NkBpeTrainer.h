// =============================================================================
// NKData/NkBpeTrainer.h — entraînement BPE À L'ÉCHELLE (16 k - 32 k fusions).
// -----------------------------------------------------------------------------
// POURQUOI CE FICHIER EXISTE (et pourquoi `TrainBpe` de NkTokenizer.h ne suffit
// pas) : l'entraîneur historique travaille sur le corpus MIS À PLAT et, à chaque
// fusion, relit puis réécrit ce tableau entier. Son coût est donc
// O(fusions x octets) et il plafonne d'ailleurs son entrée à 800 000 octets.
// À 600 fusions sur 800 Ko c'est supportable ; à 16 000 fusions sur 25 Mo cela
// représente ~4.10^11 opérations — hors d'atteinte. Il reste en place, intact :
// c'est lui qui a produit les tokenizers des paliers déjà entraînés, et rien ne
// doit changer leurs résultats.
//
// CE QUI CHANGE ICI — trois idées, aucune approximation (le résultat est le
// MÊME BPE, seul le chemin pour l'obtenir diffère) :
//   1. On ne travaille plus sur le corpus mais sur ses MOTS UNIQUES pondérés par
//      leur fréquence. 25 Mo de français ne contiennent que quelques centaines
//      de milliers de mots distincts : le volume à parcourir chute d'environ
//      deux ordres de grandeur, et compter une paire dans un mot vu 40 000 fois
//      coûte une addition, pas 40 000.
//   2. Les comptes de paires sont tenus À JOUR de façon incrémentale. Une fusion
//      ne touche que les mots qui contiennent réellement la paire fusionnée —
//      retrouvés par un index (paire -> mots) — au lieu de tout le corpus.
//   3. Le maximum est pris dans un TAS binaire à invalidation paresseuse, pas par
//      un balayage complet de la table des paires.
//
// HONNÊTETÉ SUR L'ÉQUIVALENCE : à égalité de compte, l'ordre de départage n'est
// pas celui de l'implémentation historique (elle dépendait de l'ordre de
// parcours du corpus ; ici on départage par la plus petite clé de paire, ce qui
// est déterministe et reproductible). Les deux restent de VRAIS BPE — à chaque
// étape la paire choisie est bien une paire de fréquence maximale — mais sur un
// corpus où des ex æquo existent, les listes de fusions peuvent différer. C'est
// vérifié explicitement par `verifyMerges` ci-dessous plutôt que supposé.
//
// Namespace nkentseu::ai::data. Zéro STL.
// AUTEUR : Rihen — LICENCE : Propriétaire - usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKData/NkTokenizer.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {
		namespace data {

			// ---- Réglages d'entraînement ------------------------------------------------
			struct NkBpeTrainConfig {
				public:
					// Taille de vocabulaire visée, tokens d'octets compris. Le nombre de
					// fusions apprises vaut donc `targetVocab - 256`. Les éventuels tokens
					// spéciaux du consommateur (tags de langue…) s'ajoutent PAR-DESSUS.
					int32 targetVocab = 16384;

					// On arrête si la meilleure paire restante est plus rare que ce seuil :
					// fusionner une paire vue une seule fois n'apprend rien, cela ne fait
					// qu'ajouter au vocabulaire un token qui ne resservira jamais.
					int64 minPairFreq = 2;

					// Mode de pré-tokenisation (cf NkPreTokMode). Recopié dans le NkBpe
					// produit : l'encodage DOIT employer le même.
					int32 pretok = NK_PRETOK_WORD_PUNCT;

					// Vérification par force brute des `verifyMerges` premières fusions :
					// on recalcule TOUS les comptes de paires depuis zéro et on contrôle
					// (a) que la table incrémentale dit exactement la même chose, (b) que la
					// paire choisie est bien de compte maximal. Coûteux (chaque contrôle
					// reparcourt tous les mots) donc réservé aux tests — mais c'est la seule
					// façon de PROUVER la comptabilité incrémentale au lieu de la croire.
					int32 verifyMerges = 0;

					bool verbose = true;
			};

			// ---- Ce que l'entraînement a réellement fait --------------------------------
			struct NkBpeTrainStats {
				public:
					int64 totalBytes = 0;	 // octets de corpus lus
					int64 uniqueWords = 0;	 // mots distincts après pré-tokenisation
					int64 wordOccur = 0;	 // occurrences de mots (somme des fréquences)
					int64 initialSymbols = 0;// symboles à plat des mots uniques
					int64 merges = 0;		 // fusions réellement apprises
					int64 finalVocab = 0;	 // 256 + merges
					// Nombre de tokens que représente le corpus dans l'état FINAL interne de
					// l'entraîneur (somme des longueurs de mots pondérées par leur fréquence).
					// Sert de point de contrôle : encoder le corpus avec le tokenizer produit
					// doit donner EXACTEMENT ce nombre. Sans cette confrontation, une erreur
					// qui corromprait l'état des mots resterait invisible — les comptes de
					// paires resteraient cohérents avec un état pourtant faux.
					int64 finalSymbolsWeighted = 0;
					int64 lastPairFreq = 0;	 // fréquence de la dernière fusion (indique la saturation)
					int64 compactions = 0;	 // passes de compactage de l'index paire -> mots
					int64 verifyChecked = 0; // fusions vérifiées par force brute
					int64 verifyFailed = 0;	 // désaccords constatés (doit rester 0)
			};

			// Entraîne un BPE sur `texts`. Le corpus n'est PAS plafonné : c'est l'appelant
			// qui décide de la quantité de texte à fournir. Renvoie false si le corpus est
			// vide ou la configuration absurde.
			bool TrainBpeFast(const NkVector<NkString> &texts, const NkBpeTrainConfig &cfg, NkBpe &out,
							  NkBpeTrainStats *stats = nullptr);

			// ---- Persistance « NKBP » v1 -------------------------------------------------
			// Un tokenizer coûte plusieurs minutes à entraîner : il doit vivre dans son
			// propre fichier, indépendamment de tout checkpoint de modèle, pour être
			// réutilisé tel quel d'un entraînement à l'autre. Le fichier porte le mode de
			// pré-tokenisation : un tokenizer relu encode donc exactement comme à
			// l'entraînement, sans que l'appelant ait à s'en souvenir.
			bool SaveBpe(const char *path, const NkBpe &bpe);
			bool LoadBpe(const char *path, NkBpe &bpe);

			// ---- Encodeur à mémo ---------------------------------------------------------
			// `NkBpe::EncodeWord` est quadratique en la longueur du mot (à chaque tour il
			// cherche la fusion de plus petit rang parmi toutes les paires restantes).
			// Sur un corpus entier, le même mot est ré-encodé des milliers de fois pour un
			// résultat identique. Cette classe garde le résultat par mot : le coût devient
			// proportionnel au nombre de mots DISTINCTS, pas au nombre de mots.
			// Le résultat est identique à `NkBpe::Encode`, à la mémoire près.
			class NkBpeEncoder {
				public:
					explicit NkBpeEncoder(const NkBpe &bpe);

					// Encode `text` en ajoutant les identifiants à `out`.
					void Encode(const NkString &text, NkVector<int32> &out);

					int64 CacheHits() const {
						return mHits;
					}

					int64 CacheMisses() const {
						return mMisses;
					}

				private:
					// Table de hachage mot -> tranche d'identifiants, en adressage ouvert.
					int64 Slot(const NkString &w);
					void Grow();

					const NkBpe &mBpe;
					NkVector<NkString> mKey; // mot (vide = case libre)
					NkVector<uint8> mUsed;	 // 1 si la case porte un mot (un mot peut être vide)
					NkVector<int64> mOff;	 // offset dans mIds
					NkVector<int32> mLen;	 // longueur dans mIds
					NkVector<int32> mIds;	 // réservoir plat des encodages mémorisés
					int64 mMask = 0, mCount = 0;
					int64 mHits = 0, mMisses = 0;
			};

		} // namespace data
	} // namespace ai
} // namespace nkentseu
