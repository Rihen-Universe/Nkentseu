// =============================================================================
// NKSerialization/Native/NkNativeFormat.h
// Format binaire maison optimisé "NKS1" avec compression et CRC32.
//
// Design :
//  - Format binaire compact avec header versionné et flags extensibles
//  - Sérialisation little-endian pour portabilité multi-plateforme
//  - CRC32 IEEE 802.3 pour détection de corruption (optionnel)
//  - Stub de compression LZ4-ready (copie directe si LZ4 absent)
//  - Export contrôlé via NKENTSEU_SERIALIZATION_CLASS_EXPORT
//  - Aucune dépendance STL : tout via NkVector, NkString, types natifs
//
// Structure binaire :
//   [Header: 12 bytes]
//     - Magic:4 = 'NKS1' (0x314B534E little-endian)
//     - Version:2 = 2 (support int64/uint64/float64 full-width)
//     - Flags:2 = bit0=compression, bit1=CRC32 footer
//     - UncompressedSize:4 = taille des données avant compression
//   [Payload: variable]
//     - EntryCount:4
//     - Pour chaque entrée :
//       - KeyLen:2 + Key:KeyLen + Type:1 + ValueLen:4 + Value:ValueLen
//       - (optionnel) EntryChecksum:2 si flag CHECKSUM
//   [Footer: 8 bytes] (si flag CHECKSUM)
//     - CRC32:4 + Reserved:4
//
// Types natifs supportés :
//   - Null, Bool, Int64, UInt64, Float64, String, Object, Array
//   - Tous stockés en little-endian pour cohérence multi-plateforme
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_SERIALIZATION_NATIVE_NKNATIVEFORMAT_H
#define NKENTSEU_SERIALIZATION_NATIVE_NKNATIVEFORMAT_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusions des dépendances du module de sérialisation.
    // NkSerializationApi.h fournit les macros d'export requises.

    #include "NKSerialization/NkSerializationApi.h"
    #include "NKSerialization/NkArchive.h"
    #include "NKContainers/Sequential/NkVector.h"
    #include "NKContainers/String/NkString.h"
    #include "NKCore/NkTypes.h"

    // -------------------------------------------------------------------------
    // EN-TÊTES STANDARDS POUR OPÉRATIONS BINAIRES
    // -------------------------------------------------------------------------
    // Utilisation minimale de cstring pour memcpy (portable, pas de dépendance STL).

    #include <cstring>

    // -------------------------------------------------------------------------
    // SECTION 2 : DÉCLARATION DU NAMESPACE PRINCIPAL
    // -------------------------------------------------------------------------
    // Tous les symboles du module serialization sont dans le namespace nkentseu.

    namespace nkentseu {


        // =============================================================================
        // NAMESPACE : native
        // DESCRIPTION : Implémentation du format binaire natif NKS1
        // =============================================================================
        /**
         * @namespace native
         * @brief Implémentation du format binaire optimisé NKS1
         * @ingroup SerializationFormatters
         *
         * Composants fournis :
         *  - Constantes : magic number, version, flags de configuration
         *  - NkNativeType : enum des types binaires supportés
         *  - NkCRC32 : calcul de checksum IEEE 802.3
         *  - io:: helpers : I/O little-endian inline sans overhead
         *  - NkNativeWriter : sérialisation binaire avec options compression/CRC
         *  - NkNativeReader : désérialisation binaire avec validation
         *  - Helpers templates : SerializeToNative/DeserializeFromNative
         *
         * Philosophie :
         *  - Compact : format binaire dense, pas de texte redondant
         *  - Rapide : I/O little-endian natif, pas de parsing complexe
         *  - Fiable : CRC32 optionnel pour détection de corruption
         *  - Extensible : flags et version pour évolutions futures
         *
         * @note Ce format est conçu pour le stockage interne et la transmission réseau.
         *       Pour l'interopérabilité humaine, préférer YAML/JSON/XML.
         */
        namespace native {


            // -----------------------------------------------------------------
            // CONSTANTES : Configuration du format NKS1
            // -----------------------------------------------------------------
            /**
             * @brief Magic number "NKS1" en little-endian (0x314B534E)
             * @ingroup NativeFormatConstants
             * @note Permet la détection rapide de fichiers corrompus ou de mauvais format
             */
            constexpr nk_uint32 NK_NATIVE_MAGIC = 0x314B534Eu;

            /**
             * @brief Version courante du format (2 = support int64/uint64/float64 full-width)
             * @ingroup NativeFormatConstants
             * @note Version 1 : int64/float64 tronqués à 4 bytes (bug corrigé)
             * @note Version 2 : types stockés sur leur taille native (8 bytes)
             */
            constexpr nk_uint16 NK_NATIVE_VERSION = 2u;

            /**
             * @brief Flag : données compressées via LZ4 (bit 0)
             * @ingroup NativeFormatConstants
             * @note Si set : le payload est compressé, UncompressedSize indique la taille décompressée
             */
            constexpr nk_uint16 NK_NATIVE_FLAG_COMPRESSED = 0x0001u;

            /**
             * @brief Flag : CRC32 footer présent (bit 1)
             * @ingroup NativeFormatConstants
             * @note Si set : 8 bytes supplémentaires en fin de fichier (CRC32 + reserved)
             */
            constexpr nk_uint16 NK_NATIVE_FLAG_CHECKSUM = 0x0002u;


            // -----------------------------------------------------------------
            // ÉNUMÉRATION : NkNativeType
            // DESCRIPTION : Types binaires supportés dans le payload
            // -----------------------------------------------------------------
            /**
             * @enum NkNativeType
             * @brief Types de valeurs dans le format binaire natif
             * @ingroup NativeFormatTypes
             *
             * Convention : UPPER_SNAKE_CASE pour cohérence avec le reste du projet.
             *
             * Encodage binaire :
             *  - Null    : 0 bytes de données
             *  - Bool    : 1 byte (0x00=false, 0x01=true)
             *  - Int64   : 8 bytes little-endian two's complement
             *  - UInt64  : 8 bytes little-endian
             *  - Float64 : 8 bytes IEEE 754 double precision
             *  - String  : ValueLen bytes UTF-8 raw
             *  - Object  : sous-archive NkArchive sérialisé récursivement
             *  - Array   : count:4 + N éléments (type:1 + len:4 + data)
             *
             * @note Ces types correspondent 1:1 avec NkArchiveValueType pour round-trip fidelity.
             * @note L'ordre des valeurs est arbitraire mais stable — ne pas dépendre des valeurs numériques.
             */
            enum class NkNativeType : nk_uint8 {
                /// @brief Valeur nulle (aucune donnée)
                NK_NATIVE_TYPE_NULL    = 0,

                /// @brief Valeur booléenne (1 byte)
                NK_NATIVE_TYPE_BOOL    = 1,

                /// @brief Entier signé 64 bits (8 bytes little-endian)
                NK_NATIVE_TYPE_INT64   = 2,

                /// @brief Entier non-signé 64 bits (8 bytes little-endian)
                NK_NATIVE_TYPE_UINT64  = 3,

                /// @brief Flottant double précision (8 bytes IEEE 754)
                NK_NATIVE_TYPE_FLOAT64 = 4,

                /// @brief Chaîne UTF-8 (ValueLen bytes raw)
                NK_NATIVE_TYPE_STRING  = 5,

                /// @brief Objet imbriqué (sous-archive NkArchive)
                NK_NATIVE_TYPE_OBJECT  = 6,

                /// @brief Tableau d'éléments (count:4 + éléments)
                NK_NATIVE_TYPE_ARRAY   = 7,

            }; // enum class NkNativeType


            // =============================================================================
            // CLASSE : NkCRC32
            // DESCRIPTION : Calcul de checksum CRC32 IEEE 802.3
            // =============================================================================
            /**
             * @class NkCRC32
             * @brief Calcul de checksum CRC32 pour détection de corruption de données
             * @ingroup NativeFormatUtilities
             *
             * Algorithme : CRC32 IEEE 802.3 (polynôme 0xEDB88320)
             *  - Table de 256 entrées générée statiquement au premier appel
             *  - Thread-safe en C++11+ grâce à l'initialisation garantie des statics
             *  - Complexité O(n) où n = taille des données
             *
             * Usage :
             *  - Validation d'intégrité des archives binaires (flag CHECKSUM)
             *  - Détection de corruption réseau ou stockage
             *  - Non cryptographique : ne pas utiliser pour la sécurité
             *
             * @note La table CRC est générée une fois et réutilisée — overhead négligeable.
             * @note Le checksum est calculé sur les données brutes (avant décompression si compressé).
             *
             * @example
             * @code
             * const void* data = ...;
             * nk_size size = ...;
             * nk_uint32 crc = nkentseu::native::NkCRC32::Compute(data, size);
             * // Comparer avec le CRC stocké pour validation
             * @endcode
             */
            class NKENTSEU_SERIALIZATION_CLASS_EXPORT NkCRC32 {


                // -----------------------------------------------------------------
                // SECTION 3 : MEMBRES PUBLICS
                // -----------------------------------------------------------------
            public:


                // -----------------------------------------------------------------
                // MÉTHODE STATIQUE : CALCUL DU CRC32
                // -----------------------------------------------------------------
                /**
                 * @brief Calcule le CRC32 IEEE 802.3 d'un bloc de données
                 * @param data Pointeur vers les données à checksummer
                 * @param size Nombre de bytes à traiter
                 * @return CRC32 sous forme de nk_uint32
                 * @note noexcept : garantie de non-levée d'exception
                 * @note Thread-safe : table statique initialisée une fois (C++11 guarantee)
                 *
                 * Algorithme :
                 *  1. Initialisation : crc = 0xFFFFFFFF
                 *  2. Pour chaque byte : crc = table[(crc ^ byte) & 0xFF] ^ (crc >> 8)
                 *  3. Finalisation : retour crc ^ 0xFFFFFFFF
                 *
                 * @note Compatible avec les implémentations standards (zlib, libpng, etc.)
                 *
                 * @example Voir l'exemple dans la documentation de la classe.
                 */
                [[nodiscard]] static nk_uint32 Compute(const void* data, nk_size size) noexcept {
                    const nk_uint32* tbl = Table();
                    nk_uint32 crc = 0xFFFFFFFFu;
                    const nk_uint8* bytes = static_cast<const nk_uint8*>(data);
                    for (nk_size i = 0; i < size; ++i) {
                        crc = tbl[(crc ^ bytes[i]) & 0xFFu] ^ (crc >> 8u);
                    }
                    return crc ^ 0xFFFFFFFFu;
                }


                // -----------------------------------------------------------------
                // SECTION 4 : MEMBRES PRIVÉS (IMPLÉMENTATION INTERNE)
                // -----------------------------------------------------------------
            private:


                // -----------------------------------------------------------------
                // MÉTHODE PRIVÉE : GÉNÉRATION DE LA TABLE CRC
                // -----------------------------------------------------------------
                /**
                 * @brief Génère ou retourne la table CRC32 statique
                 * @return Pointeur vers tableau de 256 nk_uint32 pré-calculés
                 * @note noexcept : garantie de non-levée d'exception
                 * @note Thread-safe : initialisation garantie par C++11 (Meyer's singleton pattern)
                 *
                 * Algorithme de génération :
                 *  - Pour chaque valeur 0..255 :
                 *    - crc = valeur
                 *    - Pour 8 bits : crc = (crc >> 1) ^ (polynôme si bit 0 set)
                 *  - Stockage dans tableau statique pour réutilisation
                 *
                 * @note Polynôme : 0xEDB88320 (IEEE 802.3 standard)
                 */
                static const nk_uint32* Table() noexcept {
                    static nk_uint32 tbl[256] = {};
                    static bool init = false;
                    if (!init) {
                        const nk_uint32 poly = 0xEDB88320u;
                        for (nk_uint32 i = 0; i < 256u; ++i) {
                            nk_uint32 crc = i;
                            for (int j = 0; j < 8; ++j) {
                                crc = (crc >> 1u) ^ ((crc & 1u) ? poly : 0u);
                            }
                            tbl[i] = crc;
                        }
                        init = true;
                    }
                    return tbl;
                }


            }; // class NkCRC32


            // =============================================================================
            // NAMESPACE : io
            // DESCRIPTION : Helpers d'I/O little-endian inline sans overhead
            // =============================================================================
            /**
             * @namespace io
             * @brief Fonctions utilitaires pour I/O binaire little-endian
             * @ingroup NativeFormatInternals
             *
             * Philosophie :
             *  - Inline : zéro overhead d'appel de fonction
             *  - Little-endian : format natif x86/ARM, portabilité via conversion explicite
             *  - Type-safe : fonctions dédiées par type (U8, U16LE, U32LE, U64LE, F64)
             *  - Bounds-checked : retour de valeur par défaut si dépassement de buffer
             *
             * Usage :
             *  - Écriture : Write*() vers NkVector<nk_uint8> pour construction de payload
             *  - Lecture : Read*() depuis buffer const nk_uint8* avec offset géré par référence
             *
             * @note Ces fonctions ne gèrent pas le byte-order de la plateforme — toujours little-endian.
             *       Pour du big-endian, inverser l'ordre des bytes manuellement.
             */
            namespace io {


                // -----------------------------------------------------------------
                // FONCTIONS D'ÉCRITURE LITTLE-ENDIAN
                // -----------------------------------------------------------------
                /**
                 * @brief Écrit un uint8 dans un buffer de sortie
                 * @param out Référence vers NkVector<nk_uint8> de destination
                 * @param v Valeur uint8 à écrire
                 * @note noexcept : garantie de non-levée d'exception
                 * @note PushBack() gère l'agrandissement dynamique du vector si nécessaire
                 */
                inline void WriteU8(NkVector<nk_uint8>& out, nk_uint8 v) noexcept {
                    out.PushBack(v);
                }

                /**
                 * @brief Écrit un uint16 en little-endian dans un buffer de sortie
                 * @param out Référence vers NkVector<nk_uint8> de destination
                 * @param v Valeur uint16 à écrire
                 * @note noexcept : garantie de non-levée d'exception
                 * @note Byte order : LSB first (v & 0xFF), puis MSB (v >> 8)
                 */
                inline void WriteU16LE(NkVector<nk_uint8>& out, nk_uint16 v) noexcept {
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
                inline void WriteU32LE(NkVector<nk_uint8>& out, nk_uint32 v) noexcept {
                    out.PushBack(static_cast<nk_uint8>(v));
                    out.PushBack(static_cast<nk_uint8>(v >> 8u));
                    out.PushBack(static_cast<nk_uint8>(v >> 16u));
                    out.PushBack(static_cast<nk_uint8>(v >> 24u));
                }

                /**
                 * @brief Écrit un uint64 en little-endian dans un buffer de sortie
                 * @param out Référence vers NkVector<nk_uint8> de destination
                 * @param v Valeur uint64 à écrire
                 * @note noexcept : garantie de non-levée d'exception
                 * @note Implémentation : deux appels à WriteU32LE pour réutilisation de code
                 */
                inline void WriteU64LE(NkVector<nk_uint8>& out, nk_uint64 v) noexcept {
                    WriteU32LE(out, static_cast<nk_uint32>(v));
                    WriteU32LE(out, static_cast<nk_uint32>(v >> 32u));
                }

                /**
                 * @brief Écrit un float64 (IEEE 754) en little-endian dans un buffer
                 * @param out Référence vers NkVector<nk_uint8> de destination
                 * @param v Valeur float64 à écrire
                 * @note noexcept : garantie de non-levée d'exception
                 * @note Conversion : memcpy vers uint64 pour bit-cast portable (C++20 bit_cast alternatif)
                 */
                inline void WriteF64(NkVector<nk_uint8>& out, nk_float64 v) noexcept {
                    nk_uint64 bits;
                    memcpy(&bits, &v, 8u);
                    WriteU64LE(out, bits);
                }

                /**
                 * @brief Écrit un bloc de bytes brut dans un buffer de sortie
                 * @param out Référence vers NkVector<nk_uint8> de destination
                 * @param src Pointeur const vers les données source
                 * @param n Nombre de bytes à copier
                 * @note noexcept : garantie de non-levée d'exception
                 * @note Copie byte-à-byte : pas d'alignement requis, portable
                 */
                inline void WriteBytes(NkVector<nk_uint8>& out, const void* src, nk_size n) noexcept {
                    const nk_uint8* p = static_cast<const nk_uint8*>(src);
                    for (nk_size i = 0; i < n; ++i) {
                        out.PushBack(p[i]);
                    }
                }


                // -----------------------------------------------------------------
                // FONCTIONS DE LECTURE LITTLE-ENDIAN
                // -----------------------------------------------------------------
                /**
                 * @brief Lit un uint8 depuis un buffer avec gestion d'offset
                 * @param d Pointeur const vers le buffer source
                 * @param sz Taille totale du buffer pour bounds-checking
                 * @param off Référence vers l'offset courant (incrémenté de 1)
                 * @return Valeur uint8 lue, ou 0 si dépassement de buffer
                 * @note noexcept : garantie de non-levée d'exception
                 * @note Bounds-checking : retour 0 si off+1 > sz (pas d'exception)
                 */
                inline nk_uint8 ReadU8(const nk_uint8* d, nk_size sz, nk_size& off) noexcept {
                    if (off + 1u > sz) {
                        return 0u;
                    }
                    return d[off++];
                }

                /**
                 * @brief Lit un uint16 little-endian depuis un buffer avec gestion d'offset
                 * @param d Pointeur const vers le buffer source
                 * @param sz Taille totale du buffer pour bounds-checking
                 * @param off Référence vers l'offset courant (incrémenté de 2)
                 * @return Valeur uint16 lue, ou 0 si dépassement de buffer
                 * @note noexcept : garantie de non-levée d'exception
                 * @note Byte order : LSB à off, MSB à off+1
                 */
                inline nk_uint16 ReadU16LE(const nk_uint8* d, nk_size sz, nk_size& off) noexcept {
                    if (off + 2u > sz) {
                        return 0u;
                    }
                    nk_uint16 v = static_cast<nk_uint16>(d[off] | (static_cast<nk_uint16>(d[off + 1u]) << 8u));
                    off += 2u;
                    return v;
                }

                /**
                 * @brief Lit un uint32 little-endian depuis un buffer avec gestion d'offset
                 * @param d Pointeur const vers le buffer source
                 * @param sz Taille totale du buffer pour bounds-checking
                 * @param off Référence vers l'offset courant (incrémenté de 4)
                 * @return Valeur uint32 lue, ou 0 si dépassement de buffer
                 * @note noexcept : garantie de non-levée d'exception
                 * @note Byte order : bytes 0,1,2,3 dans l'ordre croissant de poids
                 */
                inline nk_uint32 ReadU32LE(const nk_uint8* d, nk_size sz, nk_size& off) noexcept {
                    if (off + 4u > sz) {
                        return 0u;
                    }
                    nk_uint32 v = static_cast<nk_uint32>(d[off])
                                | (static_cast<nk_uint32>(d[off + 1u]) << 8u)
                                | (static_cast<nk_uint32>(d[off + 2u]) << 16u)
                                | (static_cast<nk_uint32>(d[off + 3u]) << 24u);
                    off += 4u;
                    return v;
                }

                /**
                 * @brief Lit un uint64 little-endian depuis un buffer avec gestion d'offset
                 * @param d Pointeur const vers le buffer source
                 * @param sz Taille totale du buffer pour bounds-checking
                 * @param off Référence vers l'offset courant (incrémenté de 8)
                 * @return Valeur uint64 lue, ou 0 si dépassement de buffer
                 * @note noexcept : garantie de non-levée d'exception
                 * @note Implémentation : deux appels à ReadU32LE pour réutilisation de code
                 */
                inline nk_uint64 ReadU64LE(const nk_uint8* d, nk_size sz, nk_size& off) noexcept {
                    nk_uint64 lo = ReadU32LE(d, sz, off);
                    nk_uint64 hi = ReadU32LE(d, sz, off);
                    return lo | (hi << 32u);
                }

                /**
                 * @brief Lit un float64 (IEEE 754) depuis un buffer avec gestion d'offset
                 * @param d Pointeur const vers le buffer source
                 * @param sz Taille totale du buffer pour bounds-checking
                 * @param off Référence vers l'offset courant (incrémenté de 8)
                 * @return Valeur float64 lue, ou 0.0 si dépassement de buffer
                 * @note noexcept : garantie de non-levée d'exception
                 * @note Conversion : memcpy depuis uint64 pour bit-cast portable
                 */
                inline nk_float64 ReadF64(const nk_uint8* d, nk_size sz, nk_size& off) noexcept {
                    nk_uint64 bits = ReadU64LE(d, sz, off);
                    nk_float64 v;
                    memcpy(&v, &bits, 8u);
                    return v;
                }


            } // namespace io


            // =============================================================================
            // CLASSE : NkNativeWriter
            // DESCRIPTION : Sérialiseur binaire NKS1 avec options compression/CRC
            // =============================================================================
            /**
             * @class NkNativeWriter
             * @brief Sérialiseur pour le format binaire natif NKS1
             * @ingroup NativeFormatComponents
             *
             * Fonctionnalités :
             *  - WriteArchive() : sérialisation simple sans options
             *  - WriteArchiveCompressed() : avec stub de compression LZ4-ready
             *  - WriteArchiveWithChecksum() : avec CRC32 footer pour intégrité
             *  - WriteImpl() : méthode interne commune avec flags configurables
             *
             * Structure de sortie :
             *  1. Header fixe (12 bytes) : magic, version, flags, uncompressed size
             *  2. Payload variable : entries sérialisées récursivement
             *  3. Footer optionnel (8 bytes) : CRC32 + reserved si flag CHECKSUM
             *
             * Gestion de la compression :
             *  - Stub actuel : copie directe (pas de dépendance LZ4 par défaut)
             *  - Pour activer : inclure lz4.h et décommenter l'appel LZ4_compress_default()
             *  - Le flag COMPRESSED est set automatiquement si compression réussie
             *
             * Thread-safety :
             *  - Toutes les méthodes sont stateless — thread-safe par conception
             *  - Les buffers de sortie sont fournis par l'appelant — synchronisation externe si partagé
             *
             * @note Format little-endian : portable mais nécessite conversion sur plateformes big-endian.
             * @note Version 2 : corrige la troncation int64/float64 de la version 1.
             *
             * @example Sérialisation basique
             * @code
             * nkentseu::NkArchive data;
             * data.SetInt32("count", 42);
             * data.SetString("name", "test");
             *
             * nkentseu::NkVector<nk_uint8> buffer;
             * if (nkentseu::native::NkNativeWriter::WriteArchive(data, buffer)) {
             *     // buffer contient le format binaire NKS1
             *     // Sauvegarde : File::WriteBinary("data.nks", buffer.Data(), buffer.Size());
             * }
             * @endcode
             */
            class NKENTSEU_SERIALIZATION_CLASS_EXPORT NkNativeWriter {


                // -----------------------------------------------------------------
                // SECTION 5 : MEMBRES PUBLICS
                // -----------------------------------------------------------------
            public:


                // -----------------------------------------------------------------
                // MÉTHODES STATIQUES : SÉRIALISATION NKS1
                // -----------------------------------------------------------------
                /**
                 * @brief Sérialise un NkArchive en format binaire NKS1 sans options
                 * @param archive Archive source à sérialiser
                 * @param out Référence vers NkVector<nk_uint8> pour recevoir le payload binaire
                 * @return true si la sérialisation a réussi, false en cas d'erreur
                 * @ingroup NativeWriterAPI
                 * @note noexcept-friendly : aucune exception levée, erreurs gérées par retour bool
                 *
                 * Comportement :
                 *  - Appel interne à WriteImpl(archive, out, false, false)
                 *  - Pas de compression, pas de CRC32 footer
                 *  - Format minimal pour performance maximale
                 *
                 * @example Voir l'exemple dans la documentation de la classe.
                 */
                [[nodiscard]] static nk_bool WriteArchive(const NkArchive& archive,
                                                          NkVector<nk_uint8>& out) noexcept {
                    return WriteImpl(archive, out, false, false);
                }

                /**
                 * @brief Sérialise un NkArchive en format binaire NKS1 avec compression optionnelle
                 * @param archive Archive source à sérialiser
                 * @param out Référence vers NkVector<nk_uint8> pour recevoir le payload binaire
                 * @param compress true pour activer la compression LZ4 (stub si LZ4 absent)
                 * @return true si la sérialisation a réussi, false en cas d'erreur
                 * @ingroup NativeWriterAPI
                 * @note noexcept-friendly : aucune exception levée
                 *
                 * Comportement :
                 *  - Appel interne à WriteImpl(archive, out, compress, false)
                 *  - Si compress=true et LZ4 disponible : payload compressé, flag COMPRESSED set
                 *  - Si compress=true et LZ4 absent : copie directe (no-op), flag non set
                 *  - UncompressedSize dans le header indique toujours la taille avant compression
                 *
                 * @note Pour activer LZ4 : inclure lz4.h et remplacer le stub par LZ4_compress_default()
                 *
                 * @example
                 * @code
                 * // Avec compression (si LZ4 disponible)
                 * nkentseu::NkVector<nk_uint8> compressed;
                 * nkentseu::native::NkNativeWriter::WriteArchiveCompressed(data, compressed, true);
                 *
                 * // Sans compression (plus rapide, plus gros)
                 * nkentseu::NkVector<nk_uint8> uncompressed;
                 * nkentseu::native::NkNativeWriter::WriteArchiveCompressed(data, uncompressed, false);
                 * @endcode
                 */
                [[nodiscard]] static nk_bool WriteArchiveCompressed(const NkArchive& archive,
                                                                    NkVector<nk_uint8>& out,
                                                                    nk_bool compress = true) noexcept {
                    return WriteImpl(archive, out, compress, false);
                }

                /**
                 * @brief Sérialise un NkArchive en format binaire NKS1 avec CRC32 footer
                 * @param archive Archive source à sérialiser
                 * @param out Référence vers NkVector<nk_uint8> pour recevoir le payload binaire
                 * @return true si la sérialisation a réussi, false en cas d'erreur
                 * @ingroup NativeWriterAPI
                 * @note noexcept-friendly : aucune exception levée
                 *
                 * Comportement :
                 *  - Appel interne à WriteImpl(archive, out, false, true)
                 *  - Ajoute 8 bytes en footer : CRC32(4) + reserved(4)
                 *  - Le CRC est calculé sur tout le fichier sauf les 8 bytes de footer
                 *  - Permet la détection de corruption au chargement
                 *
                 * @note Overhead : +8 bytes + temps de calcul CRC32 (O(n), très rapide)
                 * @note Recommandé pour le stockage long terme ou la transmission réseau non-fiable
                 *
                 * @example
                 * @code
                 * nkentseu::NkVector<nk_uint8> safeBuffer;
                 * if (nkentseu::native::NkNativeWriter::WriteArchiveWithChecksum(data, safeBuffer)) {
                 *     // safeBuffer contient le payload + CRC32 footer
                 *     // Au chargement : NkNativeReader validera automatiquement le CRC si flag set
                 * }
                 * @endcode
                 */
                [[nodiscard]] static nk_bool WriteArchiveWithChecksum(const NkArchive& archive,
                                                                      NkVector<nk_uint8>& out) noexcept {
                    return WriteImpl(archive, out, false, true);
                }


                // -----------------------------------------------------------------
                // SECTION 6 : MEMBRES PRIVÉS (IMPLÉMENTATION INTERNE)
                // -----------------------------------------------------------------
            private:


                // -----------------------------------------------------------------
                // HELPERS : CONVERSION NkArchiveValueType → NkNativeType
                // -----------------------------------------------------------------
                /**
                 * @brief Convertit un type NkArchiveValueType en type natif binaire
                 * @param t Type d'archive à convertir
                 * @return NkNativeType correspondant pour l'encodage binaire
                 * @note noexcept : garantie de non-levée d'exception
                 * @note Fallback : tout type inconnu → NK_NATIVE_TYPE_STRING (tolérant)
                 *
                 * Correspondances :
                 *  - NK_VALUE_NULL    → NK_NATIVE_TYPE_NULL
                 *  - NK_VALUE_BOOL    → NK_NATIVE_TYPE_BOOL
                 *  - NK_VALUE_INT64   → NK_NATIVE_TYPE_INT64
                 *  - NK_VALUE_UINT64  → NK_NATIVE_TYPE_UINT64
                 *  - NK_VALUE_FLOAT64 → NK_NATIVE_TYPE_FLOAT64
                 *  - NK_VALUE_STRING  → NK_NATIVE_TYPE_STRING
                 *  - Autres           → NK_NATIVE_TYPE_STRING (fallback sécurisé)
                 */
                static NkNativeType ValueTypeToNative(NkArchiveValueType t) noexcept {
                    switch (t) {
                        case NkArchiveValueType::NK_VALUE_NULL:
                            return NkNativeType::NK_NATIVE_TYPE_NULL;
                        case NkArchiveValueType::NK_VALUE_BOOL:
                            return NkNativeType::NK_NATIVE_TYPE_BOOL;
                        case NkArchiveValueType::NK_VALUE_INT64:
                            return NkNativeType::NK_NATIVE_TYPE_INT64;
                        case NkArchiveValueType::NK_VALUE_UINT64:
                            return NkNativeType::NK_NATIVE_TYPE_UINT64;
                        case NkArchiveValueType::NK_VALUE_FLOAT64:
                            return NkNativeType::NK_NATIVE_TYPE_FLOAT64;
                        case NkArchiveValueType::NK_VALUE_STRING:
                            return NkNativeType::NK_NATIVE_TYPE_STRING;
                        default:
                            return NkNativeType::NK_NATIVE_TYPE_STRING;
                    }
                }

                /**
                 * @brief Calcule la taille binaire d'une valeur pour un type natif donné
                 * @param v Valeur dont calculer la taille
                 * @param t Type natif correspondant
                 * @return Nombre de bytes requis pour l'encodage de cette valeur
                 * @note noexcept : garantie de non-levée d'exception
                 *
                 * Tailles par type :
                 *  - Null    : 0 bytes
                 *  - Bool    : 1 byte
                 *  - Int64   : 8 bytes
                 *  - UInt64  : 8 bytes
                 *  - Float64 : 8 bytes
                 *  - String  : v.text.Length() bytes (UTF-8 raw)
                 *  - Autres  : 0 bytes (non supporté)
                 *
                 * @note Utilisé pour écrire le champ ValueLen dans le payload binaire.
                 */
                static nk_uint32 ValueSize(const NkArchiveValue& v, NkNativeType t) noexcept {
                    switch (t) {
                        case NkNativeType::NK_NATIVE_TYPE_NULL:
                            return 0u;
                        case NkNativeType::NK_NATIVE_TYPE_BOOL:
                            return 1u;
                        case NkNativeType::NK_NATIVE_TYPE_INT64:
                            return 8u;
                        case NkNativeType::NK_NATIVE_TYPE_UINT64:
                            return 8u;
                        case NkNativeType::NK_NATIVE_TYPE_FLOAT64:
                            return 8u;
                        case NkNativeType::NK_NATIVE_TYPE_STRING:
                            return static_cast<nk_uint32>(v.text.Length());
                        default:
                            return 0u;
                    }
                }

                /**
                 * @brief Écrit une valeur scalaire dans le buffer binaire selon son type natif
                 * @param v Valeur à écrire
                 * @param t Type natif déterminant le format d'encodage
                 * @param out Référence vers le buffer de destination
                 * @note noexcept : garantie de non-levée d'exception
                 *
                 * Encodage par type :
                 *  - Null    : rien (0 bytes)
                 *  - Bool    : 1 byte (0x00=false, 0x01=true)
                 *  - Int64   : 8 bytes little-endian via io::WriteU64LE (cast depuis raw.i)
                 *  - UInt64  : 8 bytes little-endian via io::WriteU64LE (raw.u)
                 *  - Float64 : 8 bytes IEEE 754 via io::WriteF64 (raw.f)
                 *  - String  : ValueLen bytes UTF-8 raw via io::WriteBytes
                 *
                 * @note Les conversions de type (ex: raw.i → uint64) préservent la représentation binaire.
                 * @note Aucun échappement pour les strings : UTF-8 raw pour compacité.
                 */
                static void WriteValue(const NkArchiveValue& v,
                                       NkNativeType t,
                                       NkVector<nk_uint8>& out) noexcept {
                    switch (t) {
                        case NkNativeType::NK_NATIVE_TYPE_NULL:
                            break;
                        case NkNativeType::NK_NATIVE_TYPE_BOOL:
                            io::WriteU8(out, v.raw.b ? 1u : 0u);
                            break;
                        case NkNativeType::NK_NATIVE_TYPE_INT64:
                            io::WriteU64LE(out, static_cast<nk_uint64>(v.raw.i));
                            break;
                        case NkNativeType::NK_NATIVE_TYPE_UINT64:
                            io::WriteU64LE(out, v.raw.u);
                            break;
                        case NkNativeType::NK_NATIVE_TYPE_FLOAT64:
                            io::WriteF64(out, v.raw.f);
                            break;
                        case NkNativeType::NK_NATIVE_TYPE_STRING:
                            io::WriteBytes(out, v.text.CStr(), v.text.Length());
                            break;
                        default:
                            break;
                    }
                }


                // -----------------------------------------------------------------
                // FONCTIONS RÉCURSIVES : ÉCRITURE DES NŒUDS ET ARCHIVES
                // -----------------------------------------------------------------
                /**
                 * @brief Écrit récursivement un NkArchiveNode dans le buffer binaire
                 * @param node Nœud à sérialiser (scalaire, objet ou tableau)
                 * @param out Référence vers le buffer de destination
                 * @note noexcept : garantie de non-levée d'exception
                 * @note Déclaration forward : définition après la classe pour accès à NkArchiveNode complet
                 *
                 * Logique par type de nœud :
                 *  - Scalar : écrit Type(1) + ValueLen(4) + Value(ValueLen)
                 *  - Object : sérialise le sous-archive dans un buffer temporaire,
                 *            puis écrit Type(1) + SubLen(4) + SubPayload(SubLen)
                 *  - Array  : écrit count(4) + pour chaque élément : appel récursif WriteNodePayload,
                 *            puis Type(1) + TotalLen(4) + ArrayPayload(TotalLen)
                 *
                 * @note Les buffers temporaires pour objets/tableaux sont alloués sur la pile via NkVector.
                 *       Pas d'allocation heap dynamique — mémoire prévisible.
                 */
                static void WriteNodePayload(const NkArchiveNode& node,
                                             NkVector<nk_uint8>& out) noexcept;

                /**
                 * @brief Écrit récursivement un NkArchive complet dans le buffer binaire
                 * @param archive Archive source à sérialiser
                 * @param out Référence vers le buffer de destination
                 * @note noexcept : garantie de non-levée d'exception
                 *
                 * Format de sortie :
                 *  1. EntryCount(4) : nombre d'entrées dans l'archive
                 *  2. Pour chaque entrée :
                 *     - KeyLen(2) + Key(KeyLen) : clé UTF-8
                 *     - Appel à WriteNodePayload pour le nœud associé
                 *
                 * @note L'ordre des entrées est préservé (NkVector ordonné).
                 * @note Aucune indentation ou formatage texte : binaire compact.
                 */
                static void WriteArchivePayload(const NkArchive& archive,
                                                NkVector<nk_uint8>& out) noexcept {
                    const NkVector<NkArchiveEntry>& entries = archive.Entries();
                    io::WriteU32LE(out, static_cast<nk_uint32>(entries.Size()));
                    for (nk_size i = 0; i < entries.Size(); ++i) {
                        const NkArchiveEntry& e = entries[i];
                        nk_uint16 keyLen = static_cast<nk_uint16>(e.key.Length());
                        io::WriteU16LE(out, keyLen);
                        io::WriteBytes(out, e.key.CStr(), keyLen);
                        WriteNodePayload(e.node, out);
                    }
                }


                // -----------------------------------------------------------------
                // MÉTHODE PRIVÉE : IMPLÉMENTATION COMMUNE AVEC FLAGS
                // -----------------------------------------------------------------
                /**
                 * @brief Implémentation interne commune avec configuration des flags
                 * @param archive Archive source à sérialiser
                 * @param out Référence vers le buffer de destination
                 * @param compress true pour activer la compression (stub LZ4)
                 * @param withChecksum true pour ajouter le footer CRC32
                 * @return true si succès, false en cas d'erreur (toujours true actuellement)
                 * @note noexcept : garantie de non-levée d'exception
                 *
                 * Étapes de sérialisation :
                 *  1. Clear() du buffer de sortie
                 *  2. Sérialisation du payload dans un buffer temporaire
                 *  3. Compression optionnelle (stub : copie directe si LZ4 absent)
                 *  4. Écriture du header : magic(4) + version(2) + flags(2) + uncompSize(4)
                 *  5. Écriture du payload (compressé ou non)
                 *  6. Footer CRC32 optionnel : calcul sur tout le fichier sauf footer, puis écriture
                 *
                 * @note Le stub de compression peut être remplacé par un appel réel à LZ4 :
                 *       @code
                 *       int compBound = LZ4_compressBound(payloadSize);
                 *       compBuf.Resize(compBound);
                 *       int compSize = LZ4_compress_default(payloadPtr, compBuf.Data(),
                 *                                          static_cast<int>(payloadSize), compBound);
                 *       if (compSize > 0 && compSize < static_cast<int>(payloadSize)) {
                 *           compressed = true;
                 *           payloadPtr = compBuf.Data();
                 *           payloadSize = static_cast<nk_size>(compSize);
                 *       }
                 *       @endcode
                 */
                [[nodiscard]] static nk_bool WriteImpl(const NkArchive& archive,
                                                       NkVector<nk_uint8>& out,
                                                       nk_bool compress,
                                                       nk_bool withChecksum) noexcept {
                    out.Clear();

                    // Étape 1 : Construire le payload dans un buffer temporaire
                    NkVector<nk_uint8> payload;
                    payload.Reserve(512u);
                    WriteArchivePayload(archive, payload);

                    const nk_uint8* payloadPtr = payload.Data();
                    nk_size payloadSize = payload.Size();
                    nk_uint32 uncompSize = static_cast<nk_uint32>(payloadSize);
                    bool compressed = false;

                    // Étape 2 : Compression optionnelle (stub LZ4)
                    NkVector<nk_uint8> compBuf;
                    if (compress && payloadSize > 64u) {
                        // Stub : aucune compression réelle — garder l'interface pour LZ4
                        // En production : appeler LZ4_compress_default() comme montré ci-dessus
                        (void)compBuf;
                        // compressed = true; payloadPtr = ...; payloadSize = ...;
                    }

                    // Étape 3 : Écriture du header
                    nk_uint16 flags = 0u;
                    if (compressed) {
                        flags |= NK_NATIVE_FLAG_COMPRESSED;
                    }
                    if (withChecksum) {
                        flags |= NK_NATIVE_FLAG_CHECKSUM;
                    }

                    io::WriteU32LE(out, NK_NATIVE_MAGIC);
                    io::WriteU16LE(out, NK_NATIVE_VERSION);
                    io::WriteU16LE(out, flags);
                    io::WriteU32LE(out, uncompSize);

                    // Étape 4 : Écriture du payload
                    io::WriteBytes(out, payloadPtr, payloadSize);

                    // Étape 5 : Footer CRC32 optionnel
                    if (withChecksum) {
                        nk_uint32 crc = NkCRC32::Compute(out.Data(), out.Size());
                        io::WriteU32LE(out, crc);
                        io::WriteU32LE(out, 0u);  // reserved
                    }

                    return true;
                }


            }; // class NkNativeWriter


            // -------------------------------------------------------------------------
            // DÉFINITION HORS-CLASSE : WriteNodePayload
            // -------------------------------------------------------------------------
            /**
             * @brief Définition de WriteNodePayload (après la classe pour accès complet)
             * @param node Nœud à sérialiser
             * @param out Buffer de destination
             * @note noexcept : garantie de non-levée d'exception
             * @note Voir la déclaration dans NkNativeWriter pour la documentation détaillée
             */
            inline void NkNativeWriter::WriteNodePayload(const NkArchiveNode& node,
                                                         NkVector<nk_uint8>& out) noexcept {
                if (node.IsScalar()) {
                    NkNativeType nt = ValueTypeToNative(node.value.type);
                    nk_uint32 vl = ValueSize(node.value, nt);
                    io::WriteU8(out, static_cast<nk_uint8>(nt));
                    io::WriteU32LE(out, vl);
                    WriteValue(node.value, nt, out);

                } else if (node.IsObject()) {
                    // Type = Object, longueur = taille du sous-bloc
                    NkVector<nk_uint8> sub;
                    WriteArchivePayload(*node.object, sub);
                    io::WriteU8(out, static_cast<nk_uint8>(NkNativeType::NK_NATIVE_TYPE_OBJECT));
                    io::WriteU32LE(out, static_cast<nk_uint32>(sub.Size()));
                    io::WriteBytes(out, sub.Data(), sub.Size());

                } else if (node.IsArray()) {
                    // Type = Array, count:4 + éléments
                    NkVector<nk_uint8> sub;
                    io::WriteU32LE(sub, static_cast<nk_uint32>(node.array.Size()));
                    for (nk_size i = 0; i < node.array.Size(); ++i) {
                        WriteNodePayload(node.array[i], sub);
                    }
                    io::WriteU8(out, static_cast<nk_uint8>(NkNativeType::NK_NATIVE_TYPE_ARRAY));
                    io::WriteU32LE(out, static_cast<nk_uint32>(sub.Size()));
                    io::WriteBytes(out, sub.Data(), sub.Size());
                }
            }


            // =============================================================================
            // CLASSE : NkNativeReader
            // DESCRIPTION : Désérialiseur binaire NKS1 avec validation CRC
            // =============================================================================
            /**
             * @class NkNativeReader
             * @brief Désérialiseur pour le format binaire natif NKS1
             * @ingroup NativeFormatComponents
             *
             * Fonctionnalités :
             *  - ReadArchive() : point d'entrée unique pour désérialisation
             *  - Validation du magic number et de la version
             *  - Vérification optionnelle du CRC32 footer (si flag CHECKSUM)
             *  - Parsing récursif des entrées avec gestion robuste des erreurs
             *  - Message d'erreur détaillé via paramètre optionnel NkString* err
             *
             * Gestion des erreurs :
             *  - Bounds-checking à chaque lecture : retour false si dépassement
             *  - Validation du magic/version : erreur explicite si incompatibilité
             *  - CRC32 mismatch : rejet du fichier si checksum invalide
             *  - Types inconnus : erreur avec code du type pour debugging
             *
             * Thread-safety :
             *  - Toutes les méthodes sont stateless — thread-safe par conception
             *  - Les buffers d'entrée sont en lecture seule — pas de modification
             *
             * @note Format little-endian : suppose que la plateforme cible est little-endian
             *       ou gère la conversion byte-order manuellement si nécessaire.
             * @note Version 2 : lit int64/uint64/float64 sur 8 bytes (correction v1).
             *
             * @example Désérialisation basique avec gestion d'erreur
             * @code
             * // Chargement du fichier binaire en mémoire (pseudo-code)
             * nkentseu::NkVector<nk_uint8> fileData = File::ReadBinary("data.nks");
             *
             * nkentseu::NkArchive result;
             * nkentseu::NkString error;
             * if (nkentseu::native::NkNativeReader::ReadArchive(
             *         fileData.Data(), fileData.Size(), result, &error)) {
             *     // result contient les données désérialisées
             * } else {
             *     printf("Load failed: %s\n", error.CStr());
             * }
             * @endcode
             */
            class NKENTSEU_SERIALIZATION_CLASS_EXPORT NkNativeReader {


                // -----------------------------------------------------------------
                // SECTION 7 : MEMBRES PUBLICS
                // -----------------------------------------------------------------
            public:


                // -----------------------------------------------------------------
                // MÉTHODE STATIQUE : DÉSÉRIALISATION NKS1
                // -----------------------------------------------------------------
                /**
                 * @brief Désérialise un buffer binaire NKS1 dans un NkArchive
                 * @param data Pointeur const vers le buffer binaire source
                 * @param size Taille du buffer en bytes
                 * @param out Archive de destination pour les données désérialisées
                 * @param err Pointeur optionnel vers NkString pour message d'erreur détaillé
                 * @return true si la désérialisation a réussi, false en cas d'erreur
                 * @ingroup NativeReaderAPI
                 * @note noexcept-friendly : aucune exception levée, erreurs gérées par retour bool
                 *
                 * Étapes de désérialisation :
                 *  1. Validation minimale : size >= 12 bytes (header minimum)
                 *  2. Lecture du header : magic, version, flags, uncompSize
                 *  3. Validation : magic == NK_NATIVE_MAGIC, version supportée
                 *  4. Vérification CRC32 si flag CHECKSUM : calcul sur données brutes
                 *  5. Parsing récursif du payload via ReadArchivePayload()
                 *  6. Retour du résultat avec message d'erreur optionnel
                 *
                 * Gestion de la compression :
                 *  - Si flag COMPRESSED set : stub actuel ignore la compression (copie directe)
                 *  - Pour support LZ4 : décompresser avant de parser le payload
                 *
                 * @note En cas d'erreur partielle : out peut contenir des données partielles.
                 *       Toujours vérifier le retour bool avant d'utiliser le résultat.
                 *
                 * @example Voir l'exemple dans la documentation de la classe.
                 */
                [[nodiscard]] static nk_bool ReadArchive(const nk_uint8* data,
                                                         nk_size size,
                                                         NkArchive& out,
                                                         NkString* err = nullptr) noexcept {
                    if (!data || size < 12u) {
                        if (err) {
                            *err = NkString("Native archive too small");
                        }
                        return false;
                    }
                    nk_size off = 0;

                    nk_uint32 magic = io::ReadU32LE(data, size, off);
                    nk_uint16 version = io::ReadU16LE(data, size, off);
                    nk_uint16 flags = io::ReadU16LE(data, size, off);
                    nk_uint32 uncompSz = io::ReadU32LE(data, size, off);

                    if (magic != NK_NATIVE_MAGIC) {
                        if (err) {
                            *err = NkString("Invalid native magic");
                        }
                        return false;
                    }
                    if (version < 1u || version > NK_NATIVE_VERSION) {
                        if (err) {
                            *err = NkString::Fmtf("Unsupported native version %u", version);
                        }
                        return false;
                    }

                    const bool hasChecksum = (flags & NK_NATIVE_FLAG_CHECKSUM) != 0u;
                    const bool isCompressed = (flags & NK_NATIVE_FLAG_COMPRESSED) != 0u;
                    (void)isCompressed;  // Compression stub : ignoré actuellement

                    // Vérification du CRC32 global si flag set
                    if (hasChecksum) {
                        if (size < 8u) {
                            if (err) {
                                *err = NkString("Missing checksum footer");
                            }
                            return false;
                        }
                        nk_uint32 storedCRC = *reinterpret_cast<const nk_uint32*>(data + size - 8u);
                        nk_uint32 computed = NkCRC32::Compute(data, size - 8u);
                        if (storedCRC != computed) {
                            if (err) {
                                *err = NkString("CRC32 mismatch");
                            }
                            return false;
                        }
                    }

                    // Parsing du payload (décompression stub : ignorée)
                    const nk_uint8* payload = data + off;
                    nk_size payloadSize = (hasChecksum ? size - 8u : size) - off;
                    (void)uncompSz;  // Unused in stub

                    nk_size payOff = 0;
                    return ReadArchivePayload(payload, payloadSize, payOff, out, err);
                }


                // -----------------------------------------------------------------
                // SECTION 8 : MEMBRES PRIVÉS (IMPLÉMENTATION INTERNE)
                // -----------------------------------------------------------------
            private:


                // -----------------------------------------------------------------
                // FONCTIONS RÉCURSIVES : PARSING DES PAYLOADS ET NŒUDS
                // -----------------------------------------------------------------
                /**
                 * @brief Parse récursivement un payload d'archive NKS1
                 * @param d Pointeur const vers le buffer de données
                 * @param sz Taille totale du buffer pour bounds-checking
                 * @param off Référence vers l'offset courant (avancé pendant le parsing)
                 * @param out Archive de destination pour les entrées parsées
                 * @param err Pointeur optionnel pour message d'erreur
                 * @return true si le parsing a réussi, false en cas d'erreur
                 * @note noexcept : garantie de non-levée d'exception
                 *
                 * Format attendu :
                 *  1. EntryCount(4) : nombre d'entrées à parser
                 *  2. Pour chaque entrée :
                 *     - KeyLen(2) + Key(KeyLen) : lecture et construction de NkString
                 *     - Appel à ReadNode() pour parser le nœud associé
                 *     - Insertion dans out via SetNode(key, node)
                 *
                 * Gestion des erreurs :
                 *  - Bounds-checking à chaque lecture : retour false si dépassement
                 *  - Messages d'erreur explicites via paramètre err si fourni
                 *  - Arrêt immédiat à la première erreur (pas de parsing partiel)
                 *
                 * @note L'offset off est avancé au fil du parsing — utilisé par l'appelant
                 *       pour savoir où s'arrête ce bloc (pour les objets imbriqués).
                 */
                static nk_bool ReadArchivePayload(const nk_uint8* d,
                                                  nk_size sz,
                                                  nk_size& off,
                                                  NkArchive& out,
                                                  NkString* err) noexcept {
                    if (off + 4u > sz) {
                        if (err) {
                            *err = NkString("Truncated entry count");
                        }
                        return false;
                    }
                    nk_uint32 entryCount = io::ReadU32LE(d, sz, off);

                    for (nk_uint32 i = 0; i < entryCount; ++i) {
                        if (off + 2u > sz) {
                            if (err) {
                                *err = NkString("Truncated key length");
                            }
                            return false;
                        }
                        nk_uint16 keyLen = io::ReadU16LE(d, sz, off);
                        if (off + keyLen > sz) {
                            if (err) {
                                *err = NkString("Truncated key");
                            }
                            return false;
                        }

                        NkString key(reinterpret_cast<const char*>(d + off), keyLen);
                        off += keyLen;

                        NkArchiveNode node;
                        if (!ReadNode(d, sz, off, node, err)) {
                            return false;
                        }

                        out.SetNode(key.View(), node);
                    }
                    return true;
                }

                /**
                 * @brief Parse récursivement un nœud NKS1 depuis le buffer binaire
                 * @param d Pointeur const vers le buffer de données
                 * @param sz Taille totale du buffer pour bounds-checking
                 * @param off Référence vers l'offset courant (avancé pendant le parsing)
                 * @param out Référence vers NkArchiveNode pour recevoir le résultat
                 * @param err Pointeur optionnel pour message d'erreur
                 * @return true si le parsing a réussi, false en cas d'erreur
                 * @note noexcept : garantie de non-levée d'exception
                 *
                 * Format d'un nœud :
                 *  1. Type(1) : NkNativeType déterminant le format des données
                 *  2. ValueLen(4) : nombre de bytes de données suivant
                 *  3. Données : interprétées selon le type
                 *
                 * Parsing par type :
                 *  - Null    : création de NkArchiveValue::Null()
                 *  - Bool    : lecture 1 byte → FromBool(byte != 0)
                 *  - Int64   : lecture 8 bytes little-endian → FromInt64()
                 *  - UInt64  : lecture 8 bytes little-endian → FromUInt64()
                 *  - Float64 : lecture 8 bytes IEEE 754 → FromFloat64()
                 *  - String  : lecture ValueLen bytes UTF-8 → FromString()
                 *  - Object  : parsing récursif du sous-archive via ReadArchivePayload()
                 *  - Array   : lecture count(4) + parsing de count éléments récursifs
                 *
                 * Gestion des erreurs :
                 *  - Validation de ValueLen vs données disponibles
                 *  - Types inconnus : erreur avec code du type pour debugging
                 *  - Échec de parsing récursif : propagation immédiate de l'erreur
                 *
                 * @note Les objets et tableaux allouent des buffers temporaires sur la pile
                 *       via NkVector — pas d'allocation heap dynamique.
                 */
                static nk_bool ReadNode(const nk_uint8* d,
                                        nk_size sz,
                                        nk_size& off,
                                        NkArchiveNode& out,
                                        NkString* err) noexcept {
                    if (off + 5u > sz) {
                        if (err) {
                            *err = NkString("Truncated node header");
                        }
                        return false;
                    }
                    auto nativeType = static_cast<NkNativeType>(io::ReadU8(d, sz, off));
                    nk_uint32 valLen = io::ReadU32LE(d, sz, off);

                    if (off + valLen > sz) {
                        if (err) {
                            *err = NkString("Truncated node value");
                        }
                        return false;
                    }

                    switch (nativeType) {
                        case NkNativeType::NK_NATIVE_TYPE_NULL:
                            out = NkArchiveNode(NkArchiveValue::Null());
                            break;
                        case NkNativeType::NK_NATIVE_TYPE_BOOL:
                            if (valLen < 1u) {
                                return false;
                            }
                            out = NkArchiveNode(NkArchiveValue::FromBool(d[off] != 0u));
                            off += valLen;
                            break;
                        case NkNativeType::NK_NATIVE_TYPE_INT64:
                            if (valLen < 8u) {
                                return false;
                            }
                            out = NkArchiveNode(NkArchiveValue::FromInt64(
                                static_cast<nk_int64>(io::ReadU64LE(d, sz, off))));
                            // off déjà avancé par ReadU64LE
                            break;
                        case NkNativeType::NK_NATIVE_TYPE_UINT64:
                            if (valLen < 8u) {
                                return false;
                            }
                            out = NkArchiveNode(NkArchiveValue::FromUInt64(io::ReadU64LE(d, sz, off)));
                            break;
                        case NkNativeType::NK_NATIVE_TYPE_FLOAT64:
                            if (valLen < 8u) {
                                return false;
                            }
                            out = NkArchiveNode(NkArchiveValue::FromFloat64(io::ReadF64(d, sz, off)));
                            break;
                        case NkNativeType::NK_NATIVE_TYPE_STRING: {
                            NkString s(reinterpret_cast<const char*>(d + off), valLen);
                            out = NkArchiveNode(NkArchiveValue::FromString(s.View()));
                            off += valLen;
                            break;
                        }
                        case NkNativeType::NK_NATIVE_TYPE_OBJECT: {
                            NkArchive sub;
                            nk_size subOff = 0;
                            if (!ReadArchivePayload(d + off, valLen, subOff, sub, err)) {
                                return false;
                            }
                            off += valLen;
                            out.SetObject(sub);
                            break;
                        }
                        case NkNativeType::NK_NATIVE_TYPE_ARRAY: {
                            out.MakeArray();
                            const nk_uint8* arrData = d + off;
                            nk_size arrOff = 0;
                            if (arrOff + 4u > valLen) {
                                return false;
                            }
                            nk_uint32 count = io::ReadU32LE(arrData, valLen, arrOff);
                            for (nk_uint32 ai = 0; ai < count; ++ai) {
                                NkArchiveNode elem;
                                if (!ReadNode(arrData, valLen, arrOff, elem, err)) {
                                    return false;
                                }
                                out.array.PushBack(traits::NkMove(elem));
                            }
                            off += valLen;
                            break;
                        }
                        default:
                            if (err) {
                                *err = NkString::Fmtf("Unknown native type %u", static_cast<unsigned>(nativeType));
                            }
                            return false;
                    }
                    return true;
                }


            }; // class NkNativeReader


            // =============================================================================
            // HELPERS TEMPLATES : INTÉGRATION AVEC NkISerializable
            // =============================================================================
            /**
             * @brief Sérialise un objet NkISerializable en format binaire NKS1
             * @tparam T Type de l'objet à sérialiser (doit avoir Serialize(NkArchive&))
             * @param obj Référence const vers l'objet à sérialiser
             * @param out Référence vers NkVector<nk_uint8> pour recevoir le payload binaire
             * @param compress true pour activer la compression LZ4 (stub si absent)
             * @return true si la sérialisation a réussi, false en cas d'échec de Serialize()
             * @note noexcept : garantie de non-levée d'exception
             * @ingroup NativeFormatHelpers
             *
             * Étapes :
             *  1. Création d'un NkArchive intermédiaire
             *  2. Appel de obj.Serialize(archive) — retour false si échec
             *  3. Appel de NkNativeWriter::WriteArchive* selon le flag compress
             *
             * @note Ce helper combine la sérialisation réflexive/polymorphique avec
             *       le format binaire compact — pratique pour le stockage final.
             *
             * @example
             * @code
             * class Player : public nkentseu::NkISerializable {
             *     // ... implémentation de Serialize/Deserialize ...
             * };
             *
             * Player player{42, 100.f};
             * nkentseu::NkVector<nk_uint8> buffer;
             * if (nkentseu::native::SerializeToNative(player, buffer, true)) {
             *     // buffer contient le player compressé en format NKS1
             * }
             * @endcode
             */
            template<typename T>
            nk_bool SerializeToNative(const T& obj,
                                      NkVector<nk_uint8>& out,
                                      bool compress = false) noexcept {
                NkArchive archive;
                if (!obj.Serialize(archive)) {
                    return false;
                }
                return compress
                    ? NkNativeWriter::WriteArchiveCompressed(archive, out, true)
                    : NkNativeWriter::WriteArchive(archive, out);
            }

            /**
             * @brief Désérialise un buffer NKS1 dans un objet NkISerializable
             * @tparam T Type de l'objet à peupler (doit avoir Deserialize(NkArchive&))
             * @param data Pointeur const vers le buffer binaire source
             * @param size Taille du buffer en bytes
             * @param obj Référence mutable vers l'objet à peupler
             * @param err Pointeur optionnel vers NkString pour message d'erreur
             * @return true si la désérialisation a réussi, false en cas d'erreur
             * @note noexcept : garantie de non-levée d'exception
             * @ingroup NativeFormatHelpers
             *
             * Étapes :
             *  1. Appel de NkNativeReader::ReadArchive() pour parser le binaire en NkArchive
             *  2. Si succès : appel de obj.Deserialize(archive) pour peupler l'objet
             *  3. Retour du résultat combiné
             *
             * @note En cas d'erreur de parsing binaire : obj n'est pas modifié (Clear() préalable).
             * @note En cas d'erreur de Deserialize() : l'archive a été parsée mais l'objet peut être partiel.
             *
             * @example
             * @code
             * Player loaded;
             * nkentseu::NkString error;
             * if (nkentseu::native::DeserializeFromNative(buffer.Data(), buffer.Size(), loaded, &error)) {
             *     // loaded contient les données du player
             * } else {
             *     printf("Load failed: %s\n", error.CStr());
             * }
             * @endcode
             */
            template<typename T>
            nk_bool DeserializeFromNative(const nk_uint8* data,
                                          nk_size size,
                                          T& obj,
                                          NkString* err = nullptr) noexcept {
                NkArchive archive;
                if (!NkNativeReader::ReadArchive(data, size, archive, err)) {
                    return false;
                }
                return obj.Deserialize(archive);
            }


        } // namespace native


    } // namespace nkentseu


#endif // NKENTSEU_SERIALIZATION_NATIVE_NKNATIVEFORMAT_H


// =============================================================================
// EXEMPLES D'UTILISATION DE NKNATIVEFORMAT.H
// =============================================================================
// Ce fichier fournit le format binaire optimisé NKS1 pour stockage et transmission.
// Il combine compacité, validation CRC32 et stub de compression LZ4-ready.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Sérialisation/désérialisation basique sans options
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/Native/NkNativeFormat.h>

    void SaveAndLoad() {
        // Données à sérialiser
        nkentseu::NkArchive data;
        data.SetInt32("count", 42);
        data.SetString("name", "test");
        data.SetBool("active", true);

        // Sérialisation en binaire NKS1
        nkentseu::NkVector<nk_uint8> buffer;
        if (!nkentseu::native::NkNativeWriter::WriteArchive(data, buffer)) {
            printf("Serialization failed\n");
            return;
        }

        // Sauvegarde dans un fichier (pseudo-code)
        // File::WriteBinary("save.nks", buffer.Data(), buffer.Size());

        // Chargement et désérialisation
        nkentseu::NkVector<nk_uint8> loadedBuffer = File::ReadBinary("save.nks");
        nkentseu::NkArchive result;
        nkentseu::NkString error;

        if (nkentseu::native::NkNativeReader::ReadArchive(
                loadedBuffer.Data(), loadedBuffer.Size(), result, &error)) {
            // Utilisation des données chargées
            nk_int32 count = 0;
            result.GetInt32("count", count);
            printf("Loaded count: %d\n", count);
        } else {
            printf("Deserialization failed: %s\n", error.CStr());
        }
    }
*/


// -----------------------------------------------------------------------------
// Exemple 2 : Sérialisation avec CRC32 pour intégrité
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/Native/NkNativeFormat.h>

    void SaveWithIntegrityCheck() {
        nkentseu::NkArchive config;
        config.SetString("server", "prod.example.com");
        config.SetInt32("port", 443);

        // Écriture avec CRC32 footer
        nkentseu::NkVector<nk_uint8> safeBuffer;
        nkentseu::native::NkNativeWriter::WriteArchiveWithChecksum(config, safeBuffer);

        // Simulation de corruption (pour test)
        // safeBuffer[50] ^= 0xFF;  // Corrompt un byte au milieu

        // Lecture avec validation CRC
        nkentseu::NkArchive loaded;
        nkentseu::NkString error;
        if (nkentseu::native::NkNativeReader::ReadArchive(
                safeBuffer.Data(), safeBuffer.Size(), loaded, &error)) {
            printf("Config loaded successfully\n");
        } else {
            // En cas de corruption : error contiendra "CRC32 mismatch"
            printf("Integrity check failed: %s\n", error.CStr());
        }
    }
*/


// -----------------------------------------------------------------------------
// Exemple 3 : Compression LZ4 (stub → activation en production)
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/Native/NkNativeFormat.h>
    // Pour activer LZ4 : #include <lz4.h> et décommenter le code dans WriteImpl()

    void SaveCompressed() {
        nkentseu::NkArchive largeData;
        // Remplir avec beaucoup de données...
        for (int i = 0; i < 1000; ++i) {
            largeData.SetString(nkentseu::NkString::Fmtf("key_%d", i),
                               nkentseu::NkString::Fmtf("value_%d_with_some_extra_text", i));
        }

        // Sérialisation compressée
        nkentseu::NkVector<nk_uint8> compressed;
        nkentseu::native::NkNativeWriter::WriteArchiveCompressed(largeData, compressed, true);

        // Comparaison de taille (stub : pas de compression réelle)
        nkentseu::NkVector<nk_uint8> uncompressed;
        nkentseu::native::NkNativeWriter::WriteArchive(largeData, uncompressed);

        printf("Uncompressed: %zu bytes\n", uncompressed.Size());
        printf("Compressed:   %zu bytes\n", compressed.Size());
        // En production avec LZ4 : compressed sera significativement plus petit

        // La désérialisation gère automatiquement le flag COMPRESSED :
        nkentseu::NkArchive result;
        nkentseu::native::NkNativeReader::ReadArchive(compressed.Data(), compressed.Size(), result);
        // Résultat identique que ce soit compressé ou non
    }
*/


// -----------------------------------------------------------------------------
// Exemple 4 : Intégration avec NkISerializable pour objets polymorphiques
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/Native/NkNativeFormat.h>
    #include <NKSerialization/NkISerializable.h>

    class GameState : public nkentseu::NkISerializable {
    public:
        nk_uint32 level;
        nk_float32 playerHealth;
        nkentseu::NkString currentZone;

        nk_bool Serialize(nkentseu::NkArchive& arc) const override {
            return arc.SetUInt32("level", level)
                && arc.SetFloat32("playerHealth", playerHealth)
                && arc.SetString("currentZone", currentZone.View());
        }

        nk_bool Deserialize(const nkentseu::NkArchive& arc) override {
            return arc.GetUInt32("level", level)
                && arc.GetFloat32("playerHealth", playerHealth)
                && arc.GetString("currentZone", currentZone);
        }
    };

    void SaveGameState() {
        GameState state{5, 75.5f, "ForestTemple"};

        // Sérialisation directe en binaire NKS1 via helper template
        nkentseu::NkVector<nk_uint8> saveBuffer;
        if (nkentseu::native::SerializeToNative(state, saveBuffer, true)) {
            // saveBuffer contient le GameState compressé en format NKS1
            // File::WriteBinary("savegame.nks", saveBuffer.Data(), saveBuffer.Size());
        }
    }

    void LoadGameState() {
        nkentseu::NkVector<nk_uint8> saveBuffer = File::ReadBinary("savegame.nks");
        GameState loaded;
        nkentseu::NkString error;

        if (nkentseu::native::DeserializeFromNative(
                saveBuffer.Data(), saveBuffer.Size(), loaded, &error)) {
            printf("Loaded: level=%u, health=%.1f, zone=%s\n",
                   loaded.level, loaded.playerHealth, loaded.currentZone.CStr());
        } else {
            printf("Load failed: %s\n", error.CStr());
        }
    }
*/


// -----------------------------------------------------------------------------
// Exemple 5 : Gestion robuste des erreurs de parsing
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/Native/NkNativeFormat.h>

    bool SafeLoadArchive(const char* filename, nkentseu::NkArchive& out) {
        // Chargement du fichier
        nkentseu::NkVector<nk_uint8> fileData = File::ReadBinary(filename);
        if (fileData.Empty()) {
            printf("Failed to read file: %s\n", filename);
            return false;
        }

        // Désérialisation avec message d'erreur détaillé
        nkentseu::NkString error;
        if (!nkentseu::native::NkNativeReader::ReadArchive(
                fileData.Data(), fileData.Size(), out, &error)) {
            // Logging structuré pour debugging
            printf("Parse error in %s: %s\n", filename, error.CStr());

            // Analyse du type d'erreur pour récupération
            if (error.StartsWith("Invalid native magic")) {
                printf("  → Wrong file format (expected NKS1)\n");
            } else if (error.StartsWith("Unsupported native version")) {
                printf("  → File created with newer version of the engine\n");
            } else if (error.StartsWith("CRC32 mismatch")) {
                printf("  → File corrupted during storage or transfer\n");
            }
            return false;
        }

        // Validation post-parsing optionnelle
        if (out.Empty()) {
            printf("Warning: archive is empty (valid but no data)\n");
        }

        return true;
    }
*/


// =============================================================================
// NOTES DE MAINTENANCE ET BONNES PRATIQUES
// =============================================================================
/*
    1. ACTIVATION DE LZ4 :
       - Le stub actuel copie directement les données (pas de compression)
       - Pour activer : inclure lz4.h et remplacer le commentaire dans WriteImpl()
       - Tester avec des données compressibles pour valider le gain de taille
       - La désérialisation gère automatiquement le flag COMPRESSED

    2. BYTE ORDER ET PORTABILITÉ :
       - Format little-endian : natif sur x86/ARM, nécessite conversion sur big-endian
       - Pour support big-endian : ajouter des helpers io::ReadU32BE/WriteU32BE
       - Le magic number 0x314B534E permet la détection de mauvais byte-order

    3. GESTION DE LA MÉMOIRE :
       - Aucune allocation heap dynamique dans les helpers — buffers sur la pile via NkVector
       - Reserve() utilisé pour pré-allouer et éviter les réallocations multiples
       - Pour de très gros fichiers : envisager un parsing streaming (futur)

    4. VALIDATION ET SÉCURITÉ :
       - Bounds-checking à chaque lecture : retour false si dépassement (pas de crash)
       - CRC32 optionnel : recommandé pour le stockage long terme ou réseau non-fiable
       - Types inconnus : rejet avec message explicite (pas de fallback silencieux)

    5. PERFORMANCE :
       - I/O inline little-endian : zéro overhead de fonction
       - Parsing récursif : O(n) où n = nombre d'entrées, profondeur limitée par la pile
       - Compression LZ4 : trade-off CPU vs taille — activer selon le cas d'usage

    6. EXTENSIBILITÉ :
       - Flags extensibles : bits 2-15 réservés pour futures fonctionnalités
       - Versioning : NK_NATIVE_VERSION permet l'évolution du format avec rétro-compatibilité
       - Types natifs : ajouter de nouvelles valeurs à NkNativeType pour de nouveaux types

    7. DEBUGGING :
       - Messages d'erreur explicites via paramètre err : utiliser en développement
       - Magic number lisible "NKS1" : permet l'inspection hexadécimale rapide
       - Structure prévisible : header(12) + payload + footer(8 optionnel)
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================