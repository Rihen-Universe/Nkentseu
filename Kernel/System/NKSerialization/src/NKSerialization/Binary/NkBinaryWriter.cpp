// =============================================================================
// NKSerialization/Binary/NkBinaryWriter.cpp
// Implémentation du sérialiseur binaire plat "NKS0" pour NkArchive.
//
// Design :
//  - Fonctions internes dans namespace anonyme pour encapsulation
//  - Écriture binaire little-endian inline sans overhead
//  - Conversion transparente des objets/tableaux en JSON texte via NkJSONWriter
//  - Pré-allocation du buffer pour éviter les réallocations multiples
//  - Aucune allocation dynamique inutile : tout via NkVector fourni par l'appelant
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#include "NKSerialization/Binary/NkBinaryWriter.h"
#include "NKSerialization/JSON/NkJSONWriter.h"


namespace nkentseu {


    // =============================================================================
    // NAMESPACE ANONYME : CONSTANTES ET HELPERS INTERNES
    // =============================================================================
    // Ces éléments sont privés au TU pour éviter la pollution de l'espace de noms.
    // Ils implémentent la logique basique d'encodage binaire NKS0.

    namespace {


        // -------------------------------------------------------------------------
        // CONSTANTES : Configuration du format NKS0
        // -------------------------------------------------------------------------
        /**
         * @brief Magic number "NKS0" en little-endian (0x304B534E)
         * @note Permet la détection rapide de fichiers NKS0 vs autres formats
         */
        constexpr nk_uint32 NK_BINARY_MAGIC = 0x304B534Eu;

        /**
         * @brief Version courante du format NKS0
         * @note Version 1 : format plat initial (pas d'évolution prévue)
         */
        constexpr nk_uint16 NK_BINARY_VERSION = 1u;


        // -------------------------------------------------------------------------
        // FONCTIONS D'ÉCRITURE LITTLE-ENDIAN INLINE
        // -------------------------------------------------------------------------
        /**
         * @brief Écrit un uint8 dans un buffer de sortie
         * @param out Référence vers NkVector<nk_uint8> de destination
         * @param v Valeur uint8 à écrire
         * @note noexcept : garantie de non-levée d'exception
         */
        void WriteU8(NkVector<nk_uint8>& out, nk_uint8 v) noexcept {
            out.PushBack(v);
        }

        /**
         * @brief Écrit un uint16 en little-endian dans un buffer de sortie
         * @param out Référence vers NkVector<nk_uint8> de destination
         * @param v Valeur uint16 à écrire
         * @note noexcept : garantie de non-levée d'exception
         * @note Byte order : LSB first, puis MSB
         */
        void WriteU16LE(NkVector<nk_uint8>& out, nk_uint16 v) noexcept {
            out.PushBack(static_cast<nk_uint8>(v));
            out.PushBack(static_cast<nk_uint8>(v >> 8u));
        }

        /**
         * @brief Écrit un uint32 en little-endian dans un buffer de sortie
         * @param out Référence vers NkVector<nk_uint8> de destination
         * @param v Valeur uint32 à écrire
         * @note noexcept : garantie de non-levée d'exception
         * @note Byte order : bytes 0,1,2,3 dans l'ordre croissant de poids
         */
        void WriteU32LE(NkVector<nk_uint8>& out, nk_uint32 v) noexcept {
            out.PushBack(static_cast<nk_uint8>(v));
            out.PushBack(static_cast<nk_uint8>(v >> 8u));
            out.PushBack(static_cast<nk_uint8>(v >> 16u));
            out.PushBack(static_cast<nk_uint8>(v >> 24u));
        }

        /**
         * @brief Écrit un bloc de bytes brut dans un buffer de sortie
         * @param out Référence vers NkVector<nk_uint8> de destination
         * @param s Pointeur const vers la chaîne source (UTF-8)
         * @param n Nombre de bytes à copier
         * @note noexcept : garantie de non-levée d'exception
         * @note Copie byte-à-byte : pas d'alignement requis, portable
         */
        void WriteBytes(NkVector<nk_uint8>& out, const char* s, nk_size n) noexcept {
            for (nk_size i = 0; i < n; ++i) {
                out.PushBack(static_cast<nk_uint8>(s[i]));
            }
        }


        // -------------------------------------------------------------------------
        // FONCTION : NodeToText
        // DESCRIPTION : Convertit un NkArchiveNode en texte pour stockage binaire
        // -------------------------------------------------------------------------
        /**
         * @brief Convertit un nœud en représentation textuelle pour le format plat
         * @param node Nœud à convertir (scalaire, objet ou tableau)
         * @return NkString contenant la représentation textuelle
         * @note noexcept : garantie de non-levée d'exception
         *
         * Logique par type de nœud :
         *  - Scalar : retourne node.value.text (représentation canonique déjà formatée)
         *  - Object : sérialise le sous-archive en JSON compact via NkJSONWriter
         *  - Array  : construit manuellement un JSON array "[val1,val2,...]"
         *
         * @note Cette conversion est nécessaire car le format NKS0 est plat :
         *       il ne peut pas stocker de structure hiérarchique binaire native.
         *       Le texte JSON sert de "conteneur" pour les données complexes.
         *
         * @example
         * @code
         * // Scalar : NodeToText({type:INT64, text:"42"}) → "42"
         * // Object : NodeToText({object:{a:1,b:2}}) → "{\"a\":1,\"b\":2}"
         * // Array  : NodeToText({array:[1,2,3]}) → "[1,2,3]"
         * @endcode
         */
        NkString NodeToText(const NkArchiveNode& node) noexcept {
            if (node.IsScalar()) {
                return node.value.text;
            }
            if (node.IsObject()) {
                NkString json;
                NkJSONWriter::WriteArchive(*node.object, json, false);
                return json;
            }
            if (node.IsArray()) {
                NkString s("[");
                for (nk_size i = 0; i < node.array.Size(); ++i) {
                    if (i > 0) {
                        s.Append(',');
                    }
                    s.Append(NodeToText(node.array[i]));
                }
                s.Append(']');
                return s;
            }
            return {};
        }

        /**
         * @brief Détermine le type binaire à stocker pour un nœud donné
         * @param node Nœud dont déterminer le type de stockage
         * @return nk_uint8 représentant le type (NkArchiveValueType ou STRING pour complexes)
         * @note noexcept : garantie de non-levée d'exception
         *
         * Règles de mapping :
         *  - Scalar : retourne node.value.type tel quel (NK_VALUE_BOOL, INT64, etc.)
         *  - Object : retourne NK_VALUE_STRING (le JSON texte est stocké comme string)
         *  - Array  : retourne NK_VALUE_STRING (le JSON texte est stocké comme string)
         *
         * @note Au chargement, le lecteur devra parser le JSON si type=STRING et
         *       que la clé correspond à un objet/tableau attendu — convention implicite.
         */
        nk_uint8 NodeToBinaryType(const NkArchiveNode& node) noexcept {
            if (node.IsScalar()) {
                return static_cast<nk_uint8>(node.value.type);
            }
            return static_cast<nk_uint8>(NkArchiveValueType::NK_VALUE_STRING);
        }


    } // namespace anonyme


    // =============================================================================
    // NkBinaryWriter — IMPLÉMENTATION DE LA MÉTHODE PUBLIQUE
    // =============================================================================

    // -------------------------------------------------------------------------
    // MÉTHODE : WriteArchive
    // DESCRIPTION : Point d'entrée public pour sérialisation binaire NKS0
    // -------------------------------------------------------------------------
    /**
     * @brief Implémentation de WriteArchive — point d'entrée public
     * @param archive Archive source à sérialiser
     * @param outData Référence vers NkVector<nk_uint8> pour recevoir le payload binaire
     * @return true si la sérialisation a réussi (toujours true dans l'implémentation actuelle)
     *
     * Étapes de sérialisation :
     *  1. Clear() du buffer de sortie
     *  2. Pré-allocation via Reserve() : estimation 16 + entries×32 bytes
     *  3. Écriture du header : magic(4) + version(2) + reserved(2) + count(4)
     *  4. Pour chaque entrée :
     *     a. Calcul de keyLen et conversion du nœud en texte via NodeToText()
     *     b. Détermination du type binaire via NodeToBinaryType()
     *     c. Écriture : keyLen(4) + type(1) + valueLen(4) + key + value
     *  5. Retour true pour indiquer le succès
     *
     * @note Méthode noexcept-friendly : aucune exception levée.
     * @note Le buffer outData est réinitialisé : appelant n'a pas besoin de Clear() préalable.
     * @note Performance : O(n × m) où n = nombre d'entrées, m = taille moyenne des valeurs JSON.
     *
     * @example Voir les exemples dans le header NkBinaryWriter.h
     */
    nk_bool NkBinaryWriter::WriteArchive(const NkArchive& archive,
                                         NkVector<nk_uint8>& outData) {
        outData.Clear();

        const auto& entries = archive.Entries();
        outData.Reserve(16u + entries.Size() * 32u);

        WriteU32LE(outData, NK_BINARY_MAGIC);
        WriteU16LE(outData, NK_BINARY_VERSION);
        WriteU16LE(outData, 0u);  // reserved
        WriteU32LE(outData, static_cast<nk_uint32>(entries.Size()));

        for (nk_size i = 0; i < entries.Size(); ++i) {
            const NkArchiveEntry& e = entries[i];
            nk_uint32 keyLen = static_cast<nk_uint32>(e.key.Length());

            NkString valueText = NodeToText(e.node);
            nk_uint32 valLen = static_cast<nk_uint32>(valueText.Length());
            nk_uint8 typeRaw = NodeToBinaryType(e.node);

            WriteU32LE(outData, keyLen);
            WriteU8(outData, typeRaw);
            WriteU32LE(outData, valLen);
            WriteBytes(outData, e.key.CStr(), keyLen);
            WriteBytes(outData, valueText.CStr(), valLen);
        }

        return true;
    }


} // namespace nkentseu


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================