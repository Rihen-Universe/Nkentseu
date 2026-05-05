// =============================================================================
// NKSerialization/Binary/NkBinaryReader.cpp
// Implémentation du désérialiseur binaire plat "NKS0" pour NkArchive.
//
// Design :
//  - Parsing token par token avec bounds-checking à chaque lecture
//  - Fonctions internes dans namespace anonyme pour encapsulation
//  - Reconstruction des types scalaires depuis leur représentation textuelle
//  - Gestion tolérante des bytes de padding en fin de fichier (non-fatal)
//  - Message d'erreur optionnel pour debugging en production
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#include "NKSerialization/Binary/NkBinaryReader.h"


namespace nkentseu {


    // =============================================================================
    // NAMESPACE ANONYME : CONSTANTES ET HELPERS INTERNES DE PARSING
    // =============================================================================
    // Ces éléments sont privés au TU pour éviter la pollution de l'espace de noms.
    // Ils implémentent la logique basique de parsing binaire NKS0.

    namespace {


        // -------------------------------------------------------------------------
        // CONSTANTES : Configuration du format NKS0
        // -------------------------------------------------------------------------
        /**
         * @brief Magic number "NKS0" en little-endian (0x304B534E)
         * @note Doit correspondre à NK_BINARY_MAGIC dans NkBinaryWriter
         */
        constexpr nk_uint32 NK_BINARY_MAGIC = 0x304B534Eu;

        /**
         * @brief Version attendue du format NKS0
         * @note Doit correspondre à NK_BINARY_VERSION dans NkBinaryWriter
         */
        constexpr nk_uint16 NK_BINARY_VERSION = 1u;


        // -------------------------------------------------------------------------
        // FONCTIONS DE LECTURE LITTLE-ENDIAN AVEC BOUNDS-CHECKING
        // -------------------------------------------------------------------------
        /**
         * @brief Lit un uint8 depuis un buffer avec gestion d'offset et bounds-checking
         * @param d Pointeur const vers le buffer source
         * @param sz Taille totale du buffer pour bounds-checking
         * @param off Référence vers l'offset courant (incrémenté de 1 en cas de succès)
         * @param out Référence vers la variable de sortie pour la valeur lue
         * @return true si la lecture a réussi, false si dépassement de buffer
         * @note noexcept : garantie de non-levée d'exception
         */
        bool ReadU8(const nk_uint8* d, nk_size sz, nk_size& off, nk_uint8& out) noexcept {
            if (off + 1u > sz) {
                return false;
            }
            out = d[off++];
            return true;
        }

        /**
         * @brief Lit un uint16 little-endian depuis un buffer avec bounds-checking
         * @param d Pointeur const vers le buffer source
         * @param sz Taille totale du buffer pour bounds-checking
         * @param off Référence vers l'offset courant (incrémenté de 2 en cas de succès)
         * @param out Référence vers la variable de sortie pour la valeur lue
         * @return true si la lecture a réussi, false si dépassement de buffer
         * @note noexcept : garantie de non-levée d'exception
         * @note Byte order : LSB à off, MSB à off+1
         */
        bool ReadU16LE(const nk_uint8* d, nk_size sz, nk_size& off, nk_uint16& out) noexcept {
            nk_uint8 b0 = 0;
            nk_uint8 b1 = 0;
            if (!ReadU8(d, sz, off, b0) || !ReadU8(d, sz, off, b1)) {
                return false;
            }
            out = static_cast<nk_uint16>(b0 | (static_cast<nk_uint16>(b1) << 8u));
            return true;
        }

        /**
         * @brief Lit un uint32 little-endian depuis un buffer avec bounds-checking
         * @param d Pointeur const vers le buffer source
         * @param sz Taille totale du buffer pour bounds-checking
         * @param off Référence vers l'offset courant (incrémenté de 4 en cas de succès)
         * @param out Référence vers la variable de sortie pour la valeur lue
         * @return true si la lecture a réussi, false si dépassement de buffer
         * @note noexcept : garantie de non-levée d'exception
         * @note Byte order : bytes 0,1,2,3 dans l'ordre croissant de poids
         */
        bool ReadU32LE(const nk_uint8* d, nk_size sz, nk_size& off, nk_uint32& out) noexcept {
            nk_uint8 b[4] = {};
            for (int i = 0; i < 4; ++i) {
                if (!ReadU8(d, sz, off, b[i])) {
                    return false;
                }
            }
            out = static_cast<nk_uint32>(b[0])
                | (static_cast<nk_uint32>(b[1]) << 8u)
                | (static_cast<nk_uint32>(b[2]) << 16u)
                | (static_cast<nk_uint32>(b[3]) << 24u);
            return true;
        }


        // -------------------------------------------------------------------------
        // FONCTION : ReconstructRaw
        // DESCRIPTION : Reconstruit l'union raw d'un NkArchiveValue depuis son texte
        // -------------------------------------------------------------------------
        /**
         * @brief Reconstruit l'union raw d'une valeur depuis sa représentation textuelle
         * @param v Référence mutable vers NkArchiveValue dont reconstruire raw
         * @note noexcept : garantie de non-levée d'exception
         *
         * Logique par type :
         *  - NK_VALUE_BOOL : "true" → raw.b=true, autre → raw.b=false
         *  - NK_VALUE_INT64 : parsing décimal via ToInt64() → raw.i
         *  - NK_VALUE_UINT64 : parsing décimal via ToUInt64() → raw.u
         *  - NK_VALUE_FLOAT64 : parsing via ToDouble() → raw.f
         *  - NK_VALUE_NULL/STRING : raw non-modifié (texte déjà correct)
         *
         * @note Cette fonction est nécessaire car le format NKS0 stocke tout en texte.
         *       La reconstruction permet d'utiliser les valeurs en mode binaire natif.
         * @note Les échecs de parsing (ex: texte non-numérique pour int64) laissent raw à 0.
         *
         * @example
         * @code
         * NkArchiveValue val;
         * val.type = NK_VALUE_INT64;
         * val.text = "42";
         * ReconstructRaw(val);  // val.raw.i == 42
         * @endcode
         */
        void ReconstructRaw(NkArchiveValue& v) noexcept {
            switch (v.type) {
                case NkArchiveValueType::NK_VALUE_BOOL:
                    v.raw.b = (v.text == NkString("true"));
                    break;
                case NkArchiveValueType::NK_VALUE_INT64:
                    v.text.ToInt64(v.raw.i);
                    break;
                case NkArchiveValueType::NK_VALUE_UINT64:
                    v.text.ToUInt64(v.raw.u);
                    break;
                case NkArchiveValueType::NK_VALUE_FLOAT64:
                    v.text.ToDouble(v.raw.f);
                    break;
                default:
                    break;  // Null/String : raw non-modifié
            }
        }


    } // namespace anonyme


    // =============================================================================
    // NkBinaryReader — IMPLÉMENTATION DE LA MÉTHODE PUBLIQUE
    // =============================================================================

    // -------------------------------------------------------------------------
    // MÉTHODE : ReadArchive
    // DESCRIPTION : Point d'entrée public pour désérialisation binaire NKS0
    // -------------------------------------------------------------------------
    /**
     * @brief Implémentation de ReadArchive — point d'entrée public
     * @param data Pointeur const vers le buffer binaire source
     * @param size Taille du buffer en bytes
     * @param outArchive Archive de destination pour les données désérialisées
     * @param outError Pointeur optionnel pour message d'erreur détaillé
     * @return true si la désérialisation a réussi, false en cas d'erreur
     *
     * Étapes de désérialisation :
     *  1. Clear() de l'archive de sortie
     *  2. Lecture du header : magic(4) + version(2) + reserved(2) + count(4)
     *  3. Validation : magic == NK_BINARY_MAGIC, version == NK_BINARY_VERSION
     *  4. Pour chaque entrée (count fois) :
     *     a. Lecture de keyLen(4), type(1), valueLen(4) avec bounds-checking
     *     b. Vérification que keyLen+valueLen <= données restantes
     *     c. Construction de la clé NkString depuis les bytes bruts
     *     d. Construction de la valeur : type depuis typeRaw, texte depuis valueLen bytes
     *     e. Reconstruction de l'union raw via ReconstructRaw()
     *     f. Insertion dans outArchive via SetValue()
     *  5. Gestion des bytes trailing : avertissement non-fatal si outError fourni
     *  6. Retour du résultat
     *
     * @note Méthode noexcept-friendly : aucune exception levée.
     * @note En cas d'erreur partielle : outArchive peut contenir des données partielles.
     *       L'appelant doit vérifier le retour bool avant d'utiliser le résultat.
     * @note Les objets/tableaux stockés en JSON texte ne sont pas re-parsés automatiquement.
     *
     * @example Voir les exemples dans le header NkBinaryReader.h
     */
    nk_bool NkBinaryReader::ReadArchive(const nk_uint8* data,
                                        nk_size size,
                                        NkArchive& outArchive,
                                        NkString* outError) {
        outArchive.Clear();

        nk_size off = 0;
        nk_uint32 magic = 0;
        nk_uint16 version = 0;
        nk_uint16 reserved = 0;
        nk_uint32 count = 0;

        // Lecture du header (12 bytes)
        if (!ReadU32LE(data, size, off, magic) ||
            !ReadU16LE(data, size, off, version) ||
            !ReadU16LE(data, size, off, reserved) ||
            !ReadU32LE(data, size, off, count)) {
            if (outError) {
                *outError = NkString("Binary payload too short");
            }
            return false;
        }
        (void)reserved;  // Unused

        // Validation du magic number
        if (magic != NK_BINARY_MAGIC) {
            if (outError) {
                *outError = NkString::Fmtf("Invalid binary magic 0x%08X", magic);
            }
            return false;
        }

        // Validation de la version
        if (version != NK_BINARY_VERSION) {
            if (outError) {
                *outError = NkString::Fmtf("Unsupported binary version %u", version);
            }
            return false;
        }

        // Parsing des entrées
        for (nk_uint32 i = 0; i < count; ++i) {
            nk_uint32 keyLen = 0;
            nk_uint8 typeRaw = 0;
            nk_uint32 valLen = 0;

            // Lecture de l'en-tête de l'entrée
            if (!ReadU32LE(data, size, off, keyLen) ||
                !ReadU8(data, size, off, typeRaw) ||
                !ReadU32LE(data, size, off, valLen)) {
                if (outError) {
                    *outError = NkString::Fmtf("Corrupted entry %u header", i);
                }
                return false;
            }

            // Bounds-checking pour le payload de l'entrée
            if (off + keyLen + valLen > size) {
                if (outError) {
                    *outError = NkString::Fmtf("Corrupted entry %u payload", i);
                }
                return false;
            }

            // Construction de la clé
            NkString key(reinterpret_cast<const char*>(data + off), keyLen);
            off += keyLen;

            // Construction de la valeur
            NkArchiveValue val;
            val.type = static_cast<NkArchiveValueType>(typeRaw);
            if (valLen > 0) {
                val.text = NkString(reinterpret_cast<const char*>(data + off), valLen);
            }
            if (val.type == NkArchiveValueType::NK_VALUE_NULL) {
                val.text.Clear();
            }
            off += valLen;

            // Reconstruction de l'union raw depuis le texte
            ReconstructRaw(val);

            // Insertion dans l'archive de sortie
            if (!outArchive.SetValue(key.View(), val)) {
                if (outError) {
                    *outError = NkString::Fmtf("Failed to store entry '%s'", key.CStr());
                }
                return false;
            }
        }

        // Gestion des bytes trailing : avertissement non-fatal
        if (off != size && outError) {
            // Pas d'erreur fatale : certains writers ajoutent du padding
            *outError = NkString();  // Clear pour indiquer "pas d'erreur"
        }

        return true;
    }


} // namespace nkentseu


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================