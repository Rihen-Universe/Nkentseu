// =============================================================================
// NkGGUFLoader.h — lecteur de format GGUF (NKAI, Phase 7, loader open-weight).
//
// GGUF (« GPT-Generated Unified Format ») est le format utilisé par llama.cpp /
// Ollama pour distribuer des modèles pré-entraînés (LLM) avec leurs poids
// quantifiés. Ce module lit l'EN-TÊTE + les MÉTADONNÉES + la liste TENSOR_INFO
// d'un fichier `.gguf` (ou d'un blob Ollama, qui EST un GGUF sous un nom sha256)
// SANS jamais charger les données tensor elles-mêmes en mémoire (elles peuvent
// peser plusieurs Go). Pour chaque tenseur, la taille attendue en octets est
// calculée à partir de son type de quantification et validée contre la taille
// réelle du fichier — mais aucun octet de donnée n'est lu ni déquantifié.
//
// Disposition binaire (spec publique ggml/llama.cpp, GGUF v2/v3) :
//   magic      : 4 octets ASCII "GGUF"
//   version    : uint32               (2 ou 3 supportés ; v1, 32-bit, refusé)
//   n_tensors  : uint64
//   n_kv       : uint64
//   metadata   : n_kv × { clé:gguf_string, type:uint32, valeur }
//   tensor_info: n_tensors × { nom:gguf_string, n_dims:uint32, dims[n_dims]:uint64,
//                               type:uint32, offset:uint64 (relatif, aligné) }
//   [padding jusqu'à l'alignement (general.alignment, défaut 32)]
//   données tensor (NON lues par ce module)
//
// gguf_string = uint64 longueur + octets bruts (PAS de terminateur nul).
//
// Portée honnête : les tailles de bloc/type par quantification (Q4_0, Q4_K, …)
// sont reprises de la spec publique ggml/llama.cpp (block_size/type_size des
// tables `type_traits` de ggml.c) — reproduites ici de mémoire, pas re-vérifiées
// octet-par-octet dans CE dépôt. Les types exotiques (IQ*, TQ*, K-quants futurs)
// sont reconnus par nom mais leur taille n'est PAS validée (sizeKnown=false).
// AUCUNE déquantification, AUCUNE inférence : ce module s'arrête à l'inspection
// structurelle. Zéro dépendance STL — NKMemory/NkVector/NkString uniquement.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace ai {
		namespace infer {

			// Types de valeur de métadonnée GGUF (gguf_metadata_value_type).
			enum class NkGGUFValueType : uint32 {
				NK_GGUF_UINT8 = 0,
				NK_GGUF_INT8 = 1,
				NK_GGUF_UINT16 = 2,
				NK_GGUF_INT16 = 3,
				NK_GGUF_UINT32 = 4,
				NK_GGUF_INT32 = 5,
				NK_GGUF_FLOAT32 = 6,
				NK_GGUF_BOOL = 7,
				NK_GGUF_STRING = 8,
				NK_GGUF_ARRAY = 9,
				NK_GGUF_UINT64 = 10,
				NK_GGUF_INT64 = 11,
				NK_GGUF_FLOAT64 = 12,
				NK_GGUF_UNKNOWN = 0xFFFFFFFFu,
			};

			// Nom lisible d'un type de valeur (pour affichage/log).
			const char *NkGGUFValueTypeName(NkGGUFValueType type);

			// Type de quantification d'un tenseur (ggml_type). Valeurs alignées sur
			// l'enum public ggml.h ; seuls les types listés ici ont une taille validée
			// par ce loader (voir NkGGUFTensorTypeName pour la liste complète connue).
			enum class NkGGUFTensorType : uint32 {
				NK_GGML_F32 = 0,
				NK_GGML_F16 = 1,
				NK_GGML_Q4_0 = 2,
				NK_GGML_Q4_1 = 3,
				NK_GGML_Q5_0 = 6,
				NK_GGML_Q5_1 = 7,
				NK_GGML_Q8_0 = 8,
				NK_GGML_Q8_1 = 9,
				NK_GGML_Q2_K = 10,
				NK_GGML_Q3_K = 11,
				NK_GGML_Q4_K = 12,
				NK_GGML_Q5_K = 13,
				NK_GGML_Q6_K = 14,
				NK_GGML_Q8_K = 15,
				NK_GGML_I8 = 24,
				NK_GGML_I16 = 25,
				NK_GGML_I32 = 26,
				NK_GGML_I64 = 27,
				NK_GGML_F64 = 28,
				NK_GGML_BF16 = 30,
			};

			// Nom lisible d'un type de tenseur brut (couvre aussi les IQ*/TQ* non
			// gérés par le calcul de taille, pour affichage seulement).
			const char *NkGGUFTensorTypeName(uint32 rawType);

			// Valeur scalaire de métadonnée (un seul type actif à la fois, cf. `type`).
			// `str` est hors union (type non trivial) et n'est valide que si
			// type == NK_GGUF_STRING.
			struct NkGGUFScalar {
					NkGGUFValueType type = NkGGUFValueType::NK_GGUF_UNKNOWN;
					union {
							uint8 u8;
							int8 i8;
							uint16 u16;
							int16 i16;
							uint32 u32;
							int32 i32;
							float32 f32;
							uint64 u64;
							int64 i64;
							float64 f64;
							nk_bool32 b32;
					};
					NkString str;

					NkGGUFScalar() : u64(0) {
					}
			};

			// Une entrée clé/valeur de métadonnée. Si `type == NK_GGUF_ARRAY`, la valeur
			// scalaire n'est pas utilisée : `arrayElemType`/`arrayLen` décrivent le
			// tableau, et `preview` contient les `kMaxArrayPreview` (8) premiers éléments
			// SEULEMENT (les tableaux comme `tokenizer.ggml.tokens` peuvent avoir des
			// dizaines de milliers d'entrées ; on ne matérialise pas tout, mais le
			// fichier est bien parcouru en entier pour rester positionné correctement).
			struct NkGGUFMetadataKV {
					NkString key;
					NkGGUFValueType type = NkGGUFValueType::NK_GGUF_UNKNOWN;
					NkGGUFScalar scalar; // valide si type != NK_GGUF_ARRAY
					NkGGUFValueType arrayElemType = NkGGUFValueType::NK_GGUF_UNKNOWN;
					uint64 arrayLen = 0;
					NkVector<NkGGUFScalar> preview; // <= kMaxArrayPreview éléments réels
			};

			// Nombre max d'éléments de tableau réellement matérialisés dans `preview`.
			constexpr uint64 kNkGGUFMaxArrayPreview = 8;

			// Description d'un tenseur (SANS ses données). `dims` est dans l'ORDRE DU
			// FICHIER (ne[0] = dimension la plus « rapide », convention ggml — PAS
			// forcément l'ordre row-major intuitif). `offset` est RELATIF au début de
			// la zone de données tensor (`NkGGUFFile::tensorDataOffset`), pas au début
			// du fichier.
			struct NkGGUFTensorInfo {
					NkString name;
					NkVector<uint64> dims;
					uint32 rawType = 0;			// valeur brute lue (peut être hors de l'enum connu)
					NkGGUFTensorType type{};		// (NkGGUFTensorType)rawType, informatif
					uint64 offset = 0;				// relatif à tensorDataOffset
					uint64 numElements = 0;		// produit des dims
					uint64 sizeBytes = 0;			// taille attendue en octets si sizeKnown
					bool sizeKnown = false;		// false = type de quantification non tabulé ici
			};

			// Résultat complet du parsing d'un fichier GGUF (en-tête + métadonnées +
			// tensor_info). `valid == false` si le parsing a échoué à un point
			// quelconque ; `error` contient alors un message. Les données tensor NE
			// SONT JAMAIS chargées : seule leur présence/taille attendue est validée
			// contre `fileSize`.
			struct NkGGUFFile {
					bool valid = false;
					NkString error;
					NkString path;

					uint32 version = 0;
					uint64 tensorCount = 0;
					uint64 metadataCount = 0;
					uint64 alignment = 32; // general.alignment si présent, sinon défaut GGUF

					NkVector<NkGGUFMetadataKV> metadata;
					NkVector<NkGGUFTensorInfo> tensors;

					uint64 tensorDataOffset = 0; // offset absolu (octets) du début des données
					uint64 fileSize = 0;
			};

			// Lecteur GGUF : parse l'en-tête + métadonnées + tensor_info de `path`.
			// Ne lit JAMAIS les données tensor (juste leur présence/taille est validée
			// via offset + type de quantification vs. taille réelle du fichier).
			class NkGGUFLoader {
				public:
					// true si `path` est un GGUF structurellement valide et a été
					// entièrement parsé (magic/version reconnus, toutes les métadonnées et
					// tous les tensor_info lus, offsets cohérents avec la taille du
					// fichier). false sinon (voir `outFile.error`).
					static bool Load(const char *path, NkGGUFFile &outFile);
			};

			// -----------------------------------------------------------------------
			// Accesseurs pratiques sur un NkGGUFFile déjà chargé.
			// -----------------------------------------------------------------------

			// Recherche une entrée de métadonnée par clé exacte (nullptr si absente).
			const NkGGUFMetadataKV *NkGGUFFindMeta(const NkGGUFFile &file, const char *key);

			// Récupère une métadonnée scalaire STRING. false si absente/type différent.
			bool NkGGUFGetString(const NkGGUFFile &file, const char *key, NkString &out);

			// Récupère une métadonnée scalaire entière (n'importe quel type entier
			// signé/non signé source, casté vers uint64). false si absente/non entière.
			bool NkGGUFGetUInt(const NkGGUFFile &file, const char *key, uint64 &out);

			// Idem, casté vers int64 (accepte aussi les types non signés).
			bool NkGGUFGetInt(const NkGGUFFile &file, const char *key, int64 &out);

			// Récupère une métadonnée scalaire flottante (FLOAT32 ou FLOAT64).
			bool NkGGUFGetFloat(const NkGGUFFile &file, const char *key, float64 &out);

			// Récupère une métadonnée scalaire BOOL.
			bool NkGGUFGetBool(const NkGGUFFile &file, const char *key, bool &out);

			// Longueur d'un tableau de métadonnée (0 si absent ou pas un tableau) —
			// utile par ex. pour `tokenizer.ggml.tokens` = taille du vocabulaire.
			uint64 NkGGUFArrayLength(const NkGGUFFile &file, const char *key);

			// -----------------------------------------------------------------------
			// Lecture ADDITIVE, INDÉPENDANTE de Load() (Jalon 3, NKLLMInferTest) :
			// NkGGUFFile ne garde qu'un APERÇU (8 premiers éléments, cf
			// kNkGGUFMaxArrayPreview) des tableaux de métadonnées — insuffisant pour
			// décoder un id de token en texte via le vocabulaire COMPLET
			// (`tokenizer.ggml.tokens`, ~150k entrées pour Qwen2.5). Cette fonction
			// refait un parcours dédié du fichier (réutilise les mêmes lecteurs bas
			// niveau que Load(), dans NkGGUFLoader.cpp) pour matérialiser un tableau
			// de chaînes ENTIER, SANS modifier Load() ni NkGGUFFile (zéro risque de
			// régression sur le jalon déjà validé). Ne lit que ce qui est nécessaire
			// pour atteindre/lire `key` (les autres clés sont sautées, pas stockées).
			// -----------------------------------------------------------------------
			bool NkGGUFReadFullStringArray(const char *path, const char *key, NkVector<NkString> &out);

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
