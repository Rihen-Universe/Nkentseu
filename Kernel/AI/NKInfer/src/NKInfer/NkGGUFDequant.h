// =============================================================================
// NkGGUFDequant.h — déquantification GGUF vers NkTensor f32 (NKInfer, Jalon 3).
//
// Prend le relais de NkGGUFLoader (qui parse la structure d'un GGUF mais ne
// matérialise AUCUNE donnée tensor) : ce module lit les octets bruts d'UN
// tenseur choisi (jamais tout le fichier) et les déquantifie en un buffer
// float32, puis un `ai::NkTensor` NK_F32 exploitable.
//
// Formats supportés (déquantification réelle, testée) :
//   - F32   : passthrough (memcpy).
//   - F16   : IEEE754 binary16 -> float32, via NkFp16.h (NKTensor, conversion
//             bit-exacte round-to-nearest-even déjà livrée — réutilisée telle
//             quelle, PAS réimplémentée ici).
//   - Q8_0  : blocs de 32 valeurs, échelle fp16 par bloc.
//   - Q4_K  : super-blocs de 256 valeurs (8 sous-blocs de 32), échelles ET
//             minimums quantifiés sur 6 bits chacun (paquet compact 12 octets),
//             plus deux échelles fp16 (super-échelle globale d/dmin).
//   - Q6_K  : super-blocs de 256 valeurs (16 sous-blocs de 16), quants signés
//             6 bits (4 bits bas + 2 bits hauts séparés), échelle int8 par
//             sous-bloc + une échelle fp16 globale.
//
// SOURCE DE RÉFÉRENCE (disposition binaire + algorithme de déquantification) :
// llama.cpp / ggml, dépôt officiel ggml-org/llama.cpp, fichiers :
//   - ggml/src/ggml-common.h   (struct block_q8_0, block_q4_K, block_q6_K,
//                                QK_K=256, K_SCALE_SIZE=12)
//   - ggml/src/ggml-quants.c   (dequantize_row_q8_0, dequantize_row_q4_K,
//                                dequantize_row_q6_K, get_scale_min_k4)
// consultés via WebFetch (raw.githubusercontent.com/ggml-org/llama.cpp/master/...)
// le 2026-07-25 pour CETTE implémentation — reproduits fidèlement ci-dessous
// (voir commentaires détaillés dans le .cpp à côté de chaque fonction), PAS
// "de mémoire" comme les tailles de bloc du loader structurel (NkGGUFLoader).
//
// Portée honnête : uniquement la déquantification bloc-par-bloc vers f32. PAS
// d'exécution de graphe/transformer, PAS de re-quantification vers un format
// GGUF (utile seulement pour un test d'auto-cohérence Q8_0, cf .cpp), et les
// types Q2_K/Q3_K/Q5_K/Q8_K/IQ*/TQ*/BF16 ne sont PAS supportés ici (renvoient
// false explicitement — jamais un résultat silencieusement faux).
//
// Zéro STL : NkVector/NkString/NkFile (NKFileSystem) uniquement.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKInfer/NkGGUFLoader.h"
#include "NKTensor/NkTensor.h"

namespace nkentseu {
	namespace ai {
		namespace infer {

			// true si `rawType` (valeur brute ggml_type, cf NkGGUFTensorInfo::rawType)
			// est un format pour lequel CE module sait réellement déquantifier
			// (F32, F16, Q8_0, Q4_K, Q6_K). false pour tout le reste.
			bool NkGGUFDequantSupported(uint32 rawType);

			// Déquantifie un buffer déjà en mémoire (aucune dépendance fichier —
			// utilisable directement sur un buffer synthétique pour tester
			// l'algorithme sans GGUF réel). `numElements` = nombre total de
			// scalaires attendus (doit être un multiple de la taille de bloc du
			// format : 32 pour Q8_0, 256 pour Q4_K/Q6_K ; 1 sinon).
			// `rawByteCount` doit couvrir au moins `numElements` déquantifiés
			// (vérifié arithmétiquement avant toute lecture). `outData` est
			// redimensionné à `numElements` et rempli en ordre (mémoire source
			// inchangé). Renvoie false (avec message dans `outError` si fourni)
			// si le type n'est pas supporté ou si le buffer est trop court.
			bool NkGGUFDequantizeRaw(uint32 rawType, const void *rawData, uint64 rawByteCount, uint64 numElements,
									 NkVector<float32> &outData, NkString *outError = nullptr);

			// Lit les octets bruts d'UN tenseur (`tensor`, déjà décrit par
			// `file` via NkGGUFLoader::Load) depuis `path`, à l'offset absolu
			// `file.tensorDataOffset + tensor.offset`, SANS lire le reste du
			// fichier. Nécessite `tensor.sizeKnown == true` (sinon la taille
			// exacte à lire n'est pas connue -> false).
			bool NkGGUFReadTensorRawBytes(const char *path, const NkGGUFFile &file, const NkGGUFTensorInfo &tensor,
										  NkVector<uint8> &outBytes, NkString *outError = nullptr);

			// Haut niveau : lit (NkGGUFReadTensorRawBytes) + déquantifie
			// (NkGGUFDequantizeRaw) un tenseur du GGUF `path`, et construit un
			// `ai::NkTensor` NK_F32 contigu. La forme du tenseur produit est
			// l'INVERSE de `tensor.dims` : ggml stocke ne[0] = dimension la
			// plus rapide/contiguë en mémoire, alors que NkTensor documente une
			// convention row-major (C-order) où la dernière dimension du
			// `Shape()` est la plus rapide -- inverser l'ORDRE DES DIMENSIONS
			// (PAS les octets, qui restent dans l'ordre mémoire d'origine) donne
			// donc la forme correcte pour un NkTensor row-major standard.
			bool NkGGUFDequantizeTensor(const char *path, const NkGGUFFile &file, const NkGGUFTensorInfo &tensor,
									   NkTensor &outTensor, NkString *outError = nullptr);

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
