// =============================================================================
// NkQwen2Tokenizer.h — tokenizer BPE byte-level de Qwen2/Qwen2.5 (NKInfer,
// chantier QLoRA, Jalon 1 — cf Documentation/notes_etape4_qlora.md §2.1).
//
// POURQUOI un second tokenizer alors que data::NkBpe existe déjà : le BPE
// maison (NKData/NkTokenizer.h) APPREND ses fusions sur un corpus et
// pré-tokenise autrement — il ne peut PAS produire les identifiants attendus
// par le modèle Qwen2.5 7B. Ici, tout est PRÉ-APPRIS et livré dans le GGUF :
//   - `tokenizer.ggml.tokens` : vocabulaire (152 064 chaînes pour Qwen2.5),
//     où les 256 octets bruts sont REMAPPÉS en codepoints unicode imprimables
//     (table GPT-2 `bytes_to_unicode` : l'octet 0x20 devient « Ġ » U+0120,
//     etc.) — c'est ce remappage qui rend le BPE « byte-level » : AUCUN texte
//     n'est intokenisable, tout octet a un token de base ;
//   - `tokenizer.ggml.merges` : fusions ordonnées « A B » (le rang = la
//     priorité : plus le rang est petit, plus la fusion est appliquée tôt).
//
// Ce module fournit :
//   - la table octet<->codepoint GPT-2 EXACTE et son inverse (spec : les
//     imprimables 33-126, 161-172, 174-255 restent eux-mêmes ; les 68 octets
//     exclus reçoivent 256+n dans l'ordre croissant des octets exclus) ;
//   - le chargement depuis des LISTES en mémoire (LoadFromLists — testable
//     sans aucun fichier) OU depuis un GGUF réel (LoadFromGGUF, via
//     NkGGUFReadFullStringArray, lecture additive sans toucher au loader) ;
//   - une table chaîne->id EXACTE (open addressing, hash FNV-1a 64 bits) qui
//     VÉRIFIE la chaîne au hit — un hash égal ne suffit JAMAIS : une
//     collision silencieuse produirait des ids faux sans erreur visible ;
//   - les fusions gloutonnes par rang minimal (Encode) et le décodage
//     (Decode) avec dé-remappage codepoint->octet ;
//   - les tokens spéciaux ChatML de Qwen (`<|endoftext|>`, `<|im_start|>`,
//     `<|im_end|>`) cherchés PAR CHAÎNE dans le vocabulaire (leurs ids ne
//     sont pas supposés), et EncodeWithSpecials qui les reconnaît
//     littéralement dans le texte d'entrée.
//
// PRÉ-TOKENISATION — portée honnête : la vraie pré-tokenisation de Qwen2 est
// une regex unicode (variante GPT-2) ; ici elle est APPROCHÉE en pur octet :
//   - contractions anglaises ('s, 't, 're, 've, 'm, 'll, 'd) ;
//   - « espace optionnel + lettres » — tout codepoint >= 0x80 compte comme
//     lettre (les octets UTF-8 de tête ET de continuation sont >= 0x80, donc
//     le classement PAR OCTET est cohérent : « école » reste un seul mot —
//     le français accentué du corpus BulkGen en dépend) ;
//   - « espace optionnel + 1 à 3 chiffres » (borne Qwen : \p{N}{1,3}) ;
//   - « espace optionnel + ponctuation » (suite d'octets ni espace, ni
//     lettre, ni chiffre) ;
//   - suites d'espaces, avec la règle GPT-2 `\s+(?!\S)` : le DERNIER espace
//     d'une suite suivie de texte part avec le token suivant.
// Cette approximation est VALIDÉE PAR ROUND-TRIP (decode(encode(x)) == x,
// garanti par construction byte-level) mais peut différer du découpage
// officiel sur des cas exotiques (unicode non-lettre >= 0x80, chiffres
// non-ASCII…) — à confronter aux ids officiels au premier essai sur le GGUF
// réel (cf Applications/NKQwenTokenizerTest, partie (c)).
//
// Zéro STL : NkString/NkVector/NKMemory uniquement. Namespace ai::infer.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace ai {
		namespace infer {

			// ---- Table de hachage int64->int64 (open addressing) --------------------
			// Reprise de l'IDIOME data::NkI64Map (NKData/NkTokenizer.h:24-34) mais
			// ré-implémentée ici : NKInfer ne dépend PAS de NKData (et on ne modifie
			// pas les dépendances d'un module existant pour un jalon additif).
			// Sémantique Put = insertion/écrasement (pas d'accumulation : on stocke
			// des rangs et des ids de fusion, pas des compteurs). Capacité = puissance
			// de 2 fixée à l'Init (aucun rehash : la taille est connue d'avance).
			struct NkQwen2I64Map {
					NkVector<int64> keys;
					NkVector<int64> vals;
					int64 mask = -1; // -1 = table non initialisée (Get renvoie def)

					void Init(int64 pow2);
					void Put(int64 k, int64 v);
					int64 Get(int64 k, int64 def) const;
			};

			// ---- Tokenizer BPE byte-level Qwen2 -------------------------------------
			class NkQwen2Tokenizer {
				public:
					// Nombre de codepoints adressables par la table inverse : la table
					// GPT-2 remappe 68 octets exclus vers 256..323, donc tout codepoint
					// MAPPÉ est < 324 (arrondi à 512 pour un index direct trivial).
					static constexpr int32 kCpTableSize = 512;

					// Charge depuis des listes EN MÉMOIRE (aucun fichier) : `tokens` =
					// vocabulaire id -> chaîne (octets déjà remappés en codepoints),
					// `merges` = lignes « A B » dans l'ordre des rangs. Les fusions dont
					// A, B ou la concaténation A+B manquent au vocabulaire sont IGNORÉES
					// (comptées dans SkippedMergeCount) : un GGUF réel n'en produit pas,
					// mais un vocab tronqué de test ne doit pas tout invalider.
					// false + message dans *err (si non nul) si `tokens` est vide.
					bool LoadFromLists(const NkVector<NkString> &tokens, const NkVector<NkString> &merges, NkString *err);

					// Charge depuis un GGUF réel : lit `tokenizer.ggml.tokens` et
					// `tokenizer.ggml.merges` en ENTIER via NkGGUFReadFullStringArray
					// (lecture additive : ne passe pas par NkGGUFLoader::Load, qui ne
					// garde qu'un aperçu de 8 éléments des tableaux), puis délègue à
					// LoadFromLists.
					bool LoadFromGGUF(const char *path, NkString *err);

					bool IsLoaded() const {
						return mTokens.Size() > 0;
					}
					int64 VocabSize() const {
						return (int64)mTokens.Size();
					}
					int64 MergeCount() const {
						return mMergeCount;
					}
					int64 SkippedMergeCount() const {
						return mSkippedMerges;
					}

					// Recherche EXACTE d'une chaîne du vocabulaire (chaîne REMAPPÉE,
					// telle qu'elle apparaît dans tokenizer.ggml.tokens). -1 si absente.
					int32 FindTokenId(const NkString &s) const;

					// Chaîne du vocabulaire pour un id (chaîne vide si id hors bornes —
					// robustesse face à un id corrompu, comme data::DecodeAll).
					const NkString &TokenString(int32 id) const;

					// Ids des tokens spéciaux ChatML (résolus PAR CHAÎNE au chargement ;
					// -1 si absents du vocabulaire — cas du mini-vocab sans spéciaux).
					int32 EndOfTextId() const {
						return mEndOfTextId;
					}
					int32 ImStartId() const {
						return mImStartId;
					}
					int32 ImEndId() const {
						return mImEndId;
					}

					// Pré-tokenisation approchée (cf en-tête de fichier) : découpe
					// `text` en segments d'octets BRUTS (pas encore remappés). Statique
					// et publique EXPRÈS : c'est la partie approximative du tokenizer,
					// elle doit être testable directement (NKQwenTokenizerTest).
					static void PreTokenize(const NkString &text, NkVector<NkString> &outSegments);

					// Encode `text` (octets bruts, UTF-8 ou non) -> ids, APPEND dans
					// `outIds`. Chaque segment de PreTokenize est remappé octet->
					// codepoint puis fusionné gloutonnement : à chaque itération, la
					// paire adjacente de RANG MINIMAL est fusionnée (le rang du fichier
					// merges est la priorité apprise — l'ordre d'application ne dépend
					// donc PAS de la position dans le mot). false si le tokenizer n'est
					// pas chargé ou si un octet n'a pas de token de base (vocabulaire
					// incomplet — impossible sur un GGUF Qwen réel).
					bool Encode(const NkString &text, NkVector<int32> &outIds) const;

					// Comme Encode, mais reconnaît d'abord les tokens spéciaux ÉCRITS
					// LITTÉRALEMENT dans `text` (« <|im_start|> »…) et émet leur id tel
					// quel (sans passer par le BPE : un token spécial est un id atomique,
					// le BPE le morcellerait en ponctuation). Le texte entre deux
					// spéciaux passe par Encode normalement.
					bool EncodeWithSpecials(const NkString &text, NkVector<int32> &outIds) const;

					// Decode ids -> texte (octets bruts) : concatène les chaînes du
					// vocabulaire en DÉ-REMAPPANT chaque codepoint vers son octet
					// d'origine. Un codepoint NON mappé (cas des tokens spéciaux ou de
					// tokens « added » hors table byte-level) est recopié TEL QUEL en
					// UTF-8 — ainsi decode(id(<|im_start|>)) == "<|im_start|>".
					// Ids hors bornes ignorés (robustesse).
					NkString Decode(const NkVector<int32> &ids) const;

					// Décode UN id (mêmes règles que Decode).
					NkString DecodeOne(int32 id) const;

				private:
					// Construit mByteToCp/mCpToByte (table GPT-2 exacte, cf en-tête).
					void BuildByteTables();
					// Construit la table chaîne->id (open addressing FNV-1a 64).
					void BuildStringToId();
					// Parse les fusions -> mMergeRank/mMergeResult.
					void BuildMerges(const NkVector<NkString> &merges);
					// Résout les ids des tokens spéciaux par chaîne.
					void ResolveSpecials();
					// Encode UN segment de pré-tokenisation (octets bruts) -> ids.
					bool EncodeSegment(const char *bytes, int64 len, NkVector<int32> &outIds) const;

					NkVector<NkString> mTokens;		// id -> chaîne remappée (copie du vocab)
					NkVector<int32> mHashSlots;		// open addressing : slot -> id (-1 = vide)
					int64 mHashMask = -1;

					int32 mByteToCp[256] = {};				 // octet -> codepoint remappé
					int32 mCpToByte[kCpTableSize] = {};		 // codepoint -> octet (-1 = non mappé)
					int32 mByteTokenId[256] = {};			 // octet -> id du token de base (-1 si absent)

					NkQwen2I64Map mMergeRank;	// (idA<<32)|idB -> rang
					NkQwen2I64Map mMergeResult; // (idA<<32)|idB -> id de la concaténation
					int64 mMergeCount = 0;
					int64 mSkippedMerges = 0;

					int32 mEndOfTextId = -1;
					int32 mImStartId = -1;
					int32 mImEndId = -1;
			};

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
