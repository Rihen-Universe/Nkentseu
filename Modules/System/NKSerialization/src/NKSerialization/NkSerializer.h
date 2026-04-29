// =============================================================================
// FICHIER  : Modules/System/NKSerialization/include/NKSerialization/NkSerializer.h
// MODULE   : NKSerialization
// AUTEUR   : Rihen
// DATE     : 2026-02-07
// VERSION  : 1.1.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Point d'entree unifie pour tous les formats de serialisation du module
//   NKSerialization. Fournit une API generique pour serialiser/deserialiser
//   des objets NkISerializable vers/depuis JSON, XML, YAML, binaire ou format
//   natif NK, avec detection automatique du format.
//
// CORRECTIONS ET AMELIORATIONS (v1.1.0) :
//   - Documentation Doxygen complete pour chaque fonction publique
//   - Correction : fclose(f) non defini → suppression (NkFile gere la fermeture)
//   - Uniformisation : traits::NkIsBaseOf utilise partout (pas de melange std::)
//   - Indentation stricte des blocs, namespaces et sections d'acces
//   - Une instruction par ligne pour lisibilite optimale
//   - Notes de thread-safety explicites
//   - Exemples d'utilisation complets en fin de fichier
//   - Separation claire texte/binaire avec helpers dedies
//
// DEPENDANCES :
//   - NKCore (types de base, traits, assertions)
//   - NKFileSystem (NkFile pour les operations fichier)
//   - NKContainers (NkString, NkVector pour les buffers)
//   - Formats : JSON, XML, YAML, Binary, Native (sous-modules internes)
// =============================================================================

#pragma once

#ifndef NKENTSEU_SERIALIZATION_NKSERIALIZER_H
#define NKENTSEU_SERIALIZATION_NKSERIALIZER_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DEPENDANCES INTERNES
    // -------------------------------------------------------------------------
    // Inclusion des composants de base de la serialisation : archive, interface,
    // et tous les lecteurs/ecriveurs pour les formats supportes.

    #include "NKSerialization/NkArchive.h"
    #include "NKSerialization/NkISerializable.h"
    #include "NKSerialization/JSON/NkJSONReader.h"
    #include "NKSerialization/JSON/NkJSONWriter.h"
    #include "NKSerialization/XML/NkXMLReader.h"
    #include "NKSerialization/XML/NkXMLWriter.h"
    #include "NKSerialization/YAML/NkYAMLReader.h"
    #include "NKSerialization/YAML/NkYAMLWriter.h"
    #include "NKSerialization/Binary/NkBinaryReader.h"
    #include "NKSerialization/Binary/NkBinaryWriter.h"
    #include "NKSerialization/Native/NkNativeFormat.h"

    // Inclusion du systeme de fichiers pour les operations Save/Load
    #include "NKFileSystem/NkFile.h"

    // Inclusion des types et traits de base pour la meta-programmation
    #include "NKCore/NkTypes.h"
    #include "NKCore/NkTraits.h"
    #include "NKCore/Assert/NkAssert.h"
    #include "NKMemory/NkUniquePtr.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : EN-TETES STANDARD MINIMAUX
    // -------------------------------------------------------------------------
    // Inclusion minimale des en-tetes standards pour les operations bas-niveau.
    // Aucune dependance STL runtime n'est utilisee pour les conteneurs.

    #include <cstdio>
    #include <cstring>
    #include <type_traits>

    // -------------------------------------------------------------------------
    // SECTION 3 : ESPACE DE NOMS PRINCIPAL
    // -------------------------------------------------------------------------
    // Declaration du namespace principal nkentseu pour le module de serialisation.

    namespace nkentseu {

        // =====================================================================
        // ENUMERATION : NkSerializationFormat
        // =====================================================================
        /**
         * @enum NkSerializationFormat
         * @brief Formats de serialisation supportes par le module
         *
         * Cette enumeration liste tous les formats disponibles pour la
         * serialisation et la deserialisation d'objets NkISerializable.
         *
         * @note NK_AUTO_DETECT utilise une combinaison d'extension de fichier
         *       et de magic bytes pour determiner le format automatiquement
         *
         * @warning Les formats binaires (NK_BINARY, NK_NATIVE) ne sont pas
         *          lisibles par un editeur de texte : utilisez les fonctions
         *          SerializeBinary/DeserializeBinary dediees
         */
        enum class NkSerializationFormat : nk_uint8 {
            /** Format JSON : texte, lisible, interopérable */
            NK_JSON = 0,

            /** Format XML : texte, structure hierarchique explicite */
            NK_XML = 1,

            /** Format YAML : texte, syntaxe concise, sensible a l'indentation */
            NK_YAML = 2,

            /** Format binaire generique : compact, non lisible, rapide */
            NK_BINARY = 3,

            /** Format natif NK : binaire optimise pour l'ecosysteme NKEntseu */
            NK_NATIVE = 4,

            /** Detection automatique via extension de fichier et magic bytes */
            NK_AUTO_DETECT = 5
        };

        // =====================================================================
        // NAMESPACE INTERNE : HELPERS DE DETECTION ET UTILITAIRES
        // =====================================================================
        /**
         * @namespace detail
         * @brief Fonctions utilitaires internes pour la serialisation
         *
         * Ce namespace contient des helpers non destines a l'usage public
         * mais utilises par l'API de haut niveau pour la detection de format,
         * la validation et les conversions internes.
         */
        namespace detail {

            /**
             * @brief Detecte le format de serialisation depuis l'extension d'un chemin
             * @param path Chemin du fichier a analyser (peut etre nullptr)
             * @return NkSerializationFormat detecte ou NK_AUTO_DETECT si indeterminable
             *
             * @note La detection est case-sensitive et ne gere pas les extensions
             *       composees (ex: "file.tar.gz" → seule ".gz" est consideree)
             *
             * @example
             * @code
             * DetectFormatFromExtension("data.json")  → NK_JSON
             * DetectFormatFromExtension("config.yaml") → NK_YAML
             * DetectFormatFromExtension("save")        → NK_AUTO_DETECT
             * @endcode
             */
            inline NkSerializationFormat DetectFormatFromExtension(
                const nk_char* path
            ) noexcept {
                if (!path) {
                    return NkSerializationFormat::NK_AUTO_DETECT;
                }

                const nk_char* dot = strrchr(path, '.');

                if (!dot) {
                    return NkSerializationFormat::NK_AUTO_DETECT;
                }

                ++dot;

                if (strcmp(dot, "json") == 0) {
                    return NkSerializationFormat::NK_JSON;
                }

                if (strcmp(dot, "xml") == 0) {
                    return NkSerializationFormat::NK_XML;
                }

                if (strcmp(dot, "yaml") == 0) {
                    return NkSerializationFormat::NK_YAML;
                }

                if (strcmp(dot, "yml") == 0) {
                    return NkSerializationFormat::NK_YAML;
                }

                if (strcmp(dot, "bin") == 0) {
                    return NkSerializationFormat::NK_BINARY;
                }

                if (strcmp(dot, "nk") == 0) {
                    return NkSerializationFormat::NK_NATIVE;
                }

                if (strcmp(dot, "nkasset") == 0) {
                    return NkSerializationFormat::NK_NATIVE;
                }

                return NkSerializationFormat::NK_AUTO_DETECT;
            }

            /**
             * @brief Detecte le format de serialisation depuis les magic bytes d'un buffer
             * @param data Pointeur vers les donnees brutes a analyser
             * @param size Taille du buffer en octets
             * @return NkSerializationFormat detecte ou NK_BINARY en fallback
             *
             * @note Les magic bytes reconnus :
             *       - "NKS1" (0x4E 0x4B 0x53 0x31) → NK_NATIVE
             *       - "NKS\0" (0x4E 0x4B 0x53 0x00) → NK_BINARY
             *       - Premier octet '{' → JSON probable
             *       - Premier octet '<' → XML probable
             *
             * @warning Cette detection est heuristique : un fichier texte peut
             *          accidentellement correspondre a un pattern binaire
             */
            inline NkSerializationFormat DetectFormatFromMagic(
                const nk_uint8* data,
                nk_size size
            ) noexcept {
                if (!data || size < 4) {
                    return NkSerializationFormat::NK_BINARY;
                }

                if (data[0] == 0x4E && data[1] == 0x4B &&
                    data[2] == 0x53 && data[3] == 0x31) {
                    return NkSerializationFormat::NK_NATIVE;
                }

                if (data[0] == 0x4E && data[1] == 0x4B &&
                    data[2] == 0x53 && data[3] == 0x00) {
                    return NkSerializationFormat::NK_BINARY;
                }

                if (size > 0 && data[0] == '{') {
                    return NkSerializationFormat::NK_JSON;
                }

                if (size > 0 && data[0] == '<') {
                    return NkSerializationFormat::NK_XML;
                }

                return NkSerializationFormat::NK_BINARY;
            }

            /**
             * @brief Verifie si un format est binaire (non-texte)
             * @param fmt Format a tester
             * @return nk_bool vrai si le format est binaire, faux sinon
             *
             * @note Utilise pour router vers les fonctions SerializeArchiveBinary
             *       ou SerializeArchiveText selon le cas
             */
            inline nk_bool IsBinaryFormat(NkSerializationFormat fmt) noexcept {
                return fmt == NkSerializationFormat::NK_BINARY ||
                       fmt == NkSerializationFormat::NK_NATIVE;
            }

        } // namespace detail

        // =====================================================================
        // API BAS NIVEAU : SERIALIZATION D'ARCHIVES
        // =====================================================================
        /**
         * @defgroup LowLevelArchiveAPI API Bas Niveau pour Archives
         * @brief Fonctions pour serialiser/deserialiser des NkArchive bruts
         *
         * Ces fonctions operent directement sur des objets NkArchive, sans
         * passer par l'interface NkISerializable. Utiles pour :
         *  - La composition manuelle d'archives
         *  - La transformation entre formats sans objet intermediaire
         *  - Les cas ou l'objet a serialiser n'implemente pas NkISerializable
         *
         * @note Thread-safety : Ces fonctions sont thread-safe si les objets
         *       NkArchive/NkString/NkVector passes en parametre ne sont pas
         *       modifies concurremment.
         */

        /**
         * @brief Serialise une archive vers un format texte (JSON/XML/YAML)
         * @param archive Reference constante vers l'archive a serialiser
         * @param format Format de sortie souhaite (doit etre un format texte)
         * @param outText Reference vers la chaine de sortie qui recevra le resultat
         * @param pretty nk_bool vrai pour un formatage lisible (indentation, sauts de ligne)
         * @param outError Pointeur optionnel vers une chaine pour les messages d'erreur
         * @return nk_bool vrai si la serialisation a reussi, faux sinon
         *
         * @note Les formats binaires (NK_BINARY, NK_NATIVE) retourneront false
         *       avec un message d'erreur dans outError si fourni
         * @note Le parametre pretty n'a d'effet que pour JSON et XML ; YAML
         *       est toujours genere avec une indentation minimale
         *
         * @example
         * @code
         * NkArchive archive;
         * archive.SetInt("score", 42);
         * archive.SetString("player", "Alice");
         *
         * NkString jsonOutput;
         * if (SerializeArchiveText(archive, NkSerializationFormat::NK_JSON, jsonOutput, true)) {
         *     printf("JSON: %s\n", jsonOutput.CStr());
         * }
         * @endcode
         */
        inline nk_bool SerializeArchiveText(
            const NkArchive& archive,
            NkSerializationFormat format,
            NkString& outText,
            nk_bool pretty = true,
            NkString* outError = nullptr
        ) noexcept {
            switch (format) {
                case NkSerializationFormat::NK_JSON:
                    return NkJSONWriter::WriteArchive(archive, outText, pretty);

                case NkSerializationFormat::NK_XML:
                    return NkXMLWriter::WriteArchive(archive, outText, pretty);

                case NkSerializationFormat::NK_YAML:
                    return NkYAMLWriter::WriteArchive(archive, outText);

                default:
                    if (outError) {
                        *outError = NkString("Use SerializeArchiveBinary() for binary formats");
                    }
                    return false;
            }
        }

        /**
         * @brief Serialise une archive vers un format binaire (Binary/Native)
         * @param archive Reference constante vers l'archive a serialiser
         * @param format Format de sortie souhaite (doit etre un format binaire)
         * @param outData Reference vers le vecteur de sortie qui recevra les octets
         * @param outError Pointeur optionnel vers une chaine pour les messages d'erreur
         * @return nk_bool vrai si la serialisation a reussi, faux sinon
         *
         * @note Les formats texte (JSON/XML/YAML) retourneront false avec un
         *       message d'erreur dans outError si fourni
         * @note NK_AUTO_DETECT est traite comme NK_NATIVE pour la serialisation binaire
         *
         * @example
         * @code
         * NkArchive archive;
         * archive.SetInt("level", 5);
         * archive.SetBool("unlocked", true);
         *
         * NkVector<nk_uint8> binaryData;
         * if (SerializeArchiveBinary(archive, NkSerializationFormat::NK_NATIVE, binaryData)) {
         *     // binaryData contient les octets du format natif NK
         * }
         * @endcode
         */
        inline nk_bool SerializeArchiveBinary(
            const NkArchive& archive,
            NkSerializationFormat format,
            NkVector<nk_uint8>& outData,
            NkString* outError = nullptr
        ) noexcept {
            switch (format) {
                case NkSerializationFormat::NK_BINARY:
                    return NkBinaryWriter::WriteArchive(archive, outData);

                case NkSerializationFormat::NK_NATIVE:
                case NkSerializationFormat::NK_AUTO_DETECT:
                    return native::NkNativeWriter::WriteArchive(archive, outData);

                default:
                    if (outError) {
                        *outError = NkString("Use SerializeArchiveText() for text formats");
                    }
                    return false;
            }
        }

        /**
         * @brief Deserialise un buffer texte vers une archive
         * @param text Vue sur la chaine de caractères contenant les donnees texte
         * @param format Format d'entree suppose (doit etre un format texte)
         * @param outArchive Reference vers l'archive de sortie qui recevra les donnees
         * @param outError Pointeur optionnel vers une chaine pour les messages d'erreur
         * @return nk_bool vrai si la deserialisation a reussi, faux sinon
         *
         * @note Les formats binaires retourneront false avec un message d'erreur
         * @note En cas d'erreur de parsing, outError contient une description si fourni
         *
         * @example
         * @code
         * NkStringView jsonInput = R"({"score":42,"player":"Alice"})";
         * NkArchive archive;
         * if (DeserializeArchiveText(jsonInput, NkSerializationFormat::NK_JSON, archive)) {
         *     nk_int32 score;
         *     if (archive.GetInt("score", score)) {
         *         printf("Loaded score: %d\n", score);
         *     }
         * }
         * @endcode
         */
        inline nk_bool DeserializeArchiveText(
            NkStringView text,
            NkSerializationFormat format,
            NkArchive& outArchive,
            NkString* outError = nullptr
        ) noexcept {
            switch (format) {
                case NkSerializationFormat::NK_JSON:
                    return NkJSONReader::ReadArchive(text, outArchive, outError);

                case NkSerializationFormat::NK_XML:
                    return NkXMLReader::ReadArchive(text, outArchive, outError);

                case NkSerializationFormat::NK_YAML:
                    return NkYAMLReader::ReadArchive(text, outArchive, outError);

                default:
                    if (outError) {
                        *outError = NkString("Use DeserializeArchiveBinary() for binary formats");
                    }
                    return false;
            }
        }

        /**
         * @brief Deserialise un buffer binaire vers une archive
         * @param data Pointeur vers les donnees binaires en entree
         * @param size Taille du buffer en octets
         * @param format Format d'entree suppose (NK_AUTO_DETECT active la detection par magic bytes)
         * @param outArchive Reference vers l'archive de sortie qui recevra les donnees
         * @param outError Pointeur optionnel vers une chaine pour les messages d'erreur
         * @return nk_bool vrai si la deserialisation a reussi, faux sinon
         *
         * @note Si format == NK_AUTO_DETECT, DetectFormatFromMagic() est appele
         *       pour determiner le format reel a partir des premiers octets
         * @note Les formats texte retourneront false avec un message d'erreur
         *
         * @example
         * @code
         * // Lecture d'un fichier binaire precedent
         * NkVector<nk_uint8> fileData = LoadFileBytes("save.nk");
         * NkArchive archive;
         * if (DeserializeArchiveBinary(
         *         fileData.Data(),
         *         fileData.Size(),
         *         NkSerializationFormat::NK_AUTO_DETECT,
         *         archive)) {
         *     // archive contient les donnees deserialisees
         * }
         * @endcode
         */
        inline nk_bool DeserializeArchiveBinary(
            const nk_uint8* data,
            nk_size size,
            NkSerializationFormat format,
            NkArchive& outArchive,
            NkString* outError = nullptr
        ) noexcept {
            if (!data || size == 0) {
                if (outError) {
                    *outError = NkString("Empty binary data");
                }
                return false;
            }

            if (format == NkSerializationFormat::NK_AUTO_DETECT) {
                format = detail::DetectFormatFromMagic(data, size);
            }

            switch (format) {
                case NkSerializationFormat::NK_BINARY:
                    return NkBinaryReader::ReadArchive(data, size, outArchive, outError);

                case NkSerializationFormat::NK_NATIVE:
                    return native::NkNativeReader::ReadArchive(data, size, outArchive, outError);

                default:
                    if (outError) {
                        *outError = NkString("Use DeserializeArchiveText() for text formats");
                    }
                    return false;
            }
        }

        // =====================================================================
        // API HAUT NIVEAU : SERIALIZATION D'OBJETS NkISerializable
        // =====================================================================
        /**
         * @defgroup HighLevelObjectAPI API Haut Niveau pour Objets Serialisables
         * @brief Fonctions templates pour serialiser/deserialiser des objets
         *
         * Ces fonctions templates operent sur tout type T derivant de
         * NkISerializable, en encapsulant la creation d'archive et l'appel
         * aux methodes Serialize()/Deserialize() de l'objet.
         *
         * @note Thread-safety : Ces fonctions sont thread-safe si l'objet T
         *       et les buffers de sortie ne sont pas modifies concurremment.
         * @note La contrainte de type est verifiee a la compilation via static_assert
         */

        /**
         * @brief Serialise un objet NkISerializable vers un format texte
         * @tparam T Type de l'objet a serialiser (doit heriter de NkISerializable)
         * @param obj Reference constante vers l'objet a serialiser
         * @param format Format de sortie souhaite (doit etre un format texte)
         * @param outText Reference vers la chaine de sortie qui recevra le resultat
         * @param pretty nk_bool vrai pour un formatage lisible (indentation, sauts de ligne)
         * @param outError Pointeur optionnel vers une chaine pour les messages d'erreur
         * @return nk_bool vrai si la serialisation a reussi, faux sinon
         *
         * @note La contrainte de type est verifiee a la compilation :
         *       static_assert(std::is_base_of<NkISerializable, T>::value, ...)
         * @note En cas d'echec de obj.Serialize(), un message est place dans outError
         *
         * @example
         * @code
         * class Player : public NkISerializable {
         * public:
         *     nk_int32 health = 100;
         *     nk_bool Serialize(NkArchive& ar) override {
         *         ar.SetInt("health", health);
         *         return true;
         *     }
         *     nk_bool Deserialize(const NkArchive& ar) override {
         *         return ar.GetInt("health", health);
         *     }
         * };
         *
         * Player player;
         * player.health = 75;
         * NkString json;
         * if (Serialize(player, NkSerializationFormat::NK_JSON, json, true)) {
         *     printf("Serialized: %s\n", json.CStr());
         * }
         * @endcode
         */
        template<typename T>
        nk_bool Serialize(
            const T& obj,
            NkSerializationFormat format,
            NkString& outText,
            nk_bool pretty = true,
            NkString* outError = nullptr
        ) noexcept {
            static_assert(
                std::is_base_of<NkISerializable, T>::value,
                "T must inherit from NkISerializable"
            );

            NkArchive archive;

            if (!obj.Serialize(archive)) {
                if (outError) {
                    *outError = NkString("Serialize() failed on object");
                }
                return false;
            }

            return SerializeArchiveText(archive, format, outText, pretty, outError);
        }

        /**
         * @brief Serialise un objet NkISerializable vers un format binaire
         * @tparam T Type de l'objet a serialiser (doit heriter de NkISerializable)
         * @param obj Reference constante vers l'objet a serialiser
         * @param format Format de sortie souhaite (doit etre un format binaire)
         * @param outData Reference vers le vecteur de sortie qui recevra les octets
         * @param outError Pointeur optionnel vers une chaine pour les messages d'erreur
         * @return nk_bool vrai si la serialisation a reussi, faux sinon
         *
         * @note La contrainte de type est verifiee a la compilation
         * @note NK_AUTO_DETECT est traite comme NK_NATIVE pour la serialisation
         *
         * @example
         * @code
         * Player player;
         * player.health = 50;
         * NkVector<nk_uint8> binary;
         * if (SerializeBinary(player, NkSerializationFormat::NK_NATIVE, binary)) {
         *     // binary contient la representation binaire de player
         * }
         * @endcode
         */
        template<typename T>
        nk_bool SerializeBinary(
            const T& obj,
            NkSerializationFormat format,
            NkVector<nk_uint8>& outData,
            NkString* outError = nullptr
        ) noexcept {
            static_assert(
                std::is_base_of<NkISerializable, T>::value,
                "T must inherit from NkISerializable"
            );

            NkArchive archive;

            if (!obj.Serialize(archive)) {
                if (outError) {
                    *outError = NkString("Serialize() failed on object");
                }
                return false;
            }

            return SerializeArchiveBinary(archive, format, outData, outError);
        }

        /**
         * @brief Deserialise un buffer texte vers un objet NkISerializable
         * @tparam T Type de l'objet cible (doit heriter de NkISerializable)
         * @param text Vue sur la chaine de caractères contenant les donnees texte
         * @param format Format d'entree suppose (doit etre un format texte)
         * @param obj Reference vers l'objet a peupler avec les donnees deserialisees
         * @param outError Pointeur optionnel vers une chaine pour les messages d'erreur
         * @return nk_bool vrai si la deserialisation a reussi, faux sinon
         *
         * @note L'objet obj doit etre constructible par defaut
         * @note En cas d'echec de DeserializeArchiveText ou obj.Deserialize(),
         *       la fonction retourne false et outError peut contenir un message
         *
         * @example
         * @code
         * NkStringView json = R"({"health":80})";
         * Player player;  // Constructeur par defaut requis
         * if (Deserialize(json, NkSerializationFormat::NK_JSON, player)) {
         *     printf("Loaded health: %d\n", player.health);
         * }
         * @endcode
         */
        template<typename T>
        nk_bool Deserialize(
            NkStringView text,
            NkSerializationFormat format,
            T& obj,
            NkString* outError = nullptr
        ) noexcept {
            static_assert(
                std::is_base_of<NkISerializable, T>::value,
                "T must inherit from NkISerializable"
            );

            NkArchive archive;

            if (!DeserializeArchiveText(text, format, archive, outError)) {
                return false;
            }

            return obj.Deserialize(archive);
        }

        /**
         * @brief Deserialise un buffer binaire vers un objet NkISerializable
         * @tparam T Type de l'objet cible (doit heriter de NkISerializable)
         * @param data Pointeur vers les donnees binaires en entree
         * @param size Taille du buffer en octets
         * @param format Format d'entree suppose (NK_AUTO_DETECT active la detection)
         * @param obj Reference vers l'objet a peupler avec les donnees deserialisees
         * @param outError Pointeur optionnel vers une chaine pour les messages d'erreur
         * @return nk_bool vrai si la deserialisation a reussi, faux sinon
         *
         * @note La contrainte de type est verifiee a la compilation
         * @note Si format == NK_AUTO_DETECT, la detection par magic bytes est appliquee
         *
         * @example
         * @code
         * // Supposons que binaryData contient un objet Player serialise en format natif
         * Player player;
         * if (DeserializeBinary(
         *         binaryData.Data(),
         *         binaryData.Size(),
         *         NkSerializationFormat::NK_AUTO_DETECT,
         *         player)) {
         *     // player est maintenant peuple avec les donnees du buffer
         * }
         * @endcode
         */
        template<typename T>
        nk_bool DeserializeBinary(
            const nk_uint8* data,
            nk_size size,
            NkSerializationFormat format,
            T& obj,
            NkString* outError = nullptr
        ) noexcept {
            static_assert(
                std::is_base_of<NkISerializable, T>::value,
                "T must inherit from NkISerializable"
            );

            NkArchive archive;

            if (!DeserializeArchiveBinary(data, size, format, archive, outError)) {
                return false;
            }

            return obj.Deserialize(archive);
        }

        // =====================================================================
        // API FICHIER : SAUVEGARDE ET CHARGEMENT DIRECTS
        // =====================================================================
        /**
         * @defgroup FileIOAPI API de Sauvegarde/Chargement Fichier
         * @brief Fonctions pour serialiser/deserialiser directement depuis/vers un fichier
         *
         * Ces fonctions combinent la serialisation d'objet et les operations
         * d'E/S fichier en une seule appel, avec gestion automatique du format
         * via l'extension du nom de fichier.
         *
         * @note Thread-safety : Ces fonctions ne sont pas thread-safe vis-a-vis
         *       du systeme de fichiers : ne pas appeler simultanement sur le
         *       meme chemin depuis plusieurs threads sans synchronisation externe
         */

        /**
         * @brief Sauvegarde un objet NkISerializable dans un fichier
         * @tparam T Type de l'objet a sauvegarder (doit heriter de NkISerializable)
         * @param obj Reference constante vers l'objet a serialiser
         * @param path Chemin du fichier de sortie (doit etre writable)
         * @param format Format de serialisation (NK_AUTO_DETECT utilise l'extension)
         * @param pretty nk_bool vrai pour un formatage lisible (formats texte uniquement)
         * @param outError Pointeur optionnel vers une chaine pour les messages d'erreur
         * @return nk_bool vrai si la sauvegarde a reussi, faux sinon
         *
         * @note Si format == NK_AUTO_DETECT, DetectFormatFromExtension() est appele
         *       ; si l'extension n'est pas reconnue, NK_NATIVE est utilise par defaut
         * @note Les formats binaires sont ecrits en mode binaire, les formats texte
         *       en mode texte (mais avec NkFileMode::NK_WRITE_BINARY pour coherence)
         * @note En cas d'erreur d'ouverture, d'ecriture ou de serialisation,
         *       outError contient un message descriptif si fourni
         *
         * @example
         * @code
         * Player player;
         * player.health = 90;
         *
         * // Sauvegarde en JSON avec formatage lisible
         * if (SaveToFile(player, "saves/player.json", NkSerializationFormat::NK_JSON, true)) {
         *     printf("Player saved to JSON\n");
         * }
         *
         * // Sauvegarde en format natif avec detection automatique par extension
         * if (SaveToFile(player, "saves/player.nk", NkSerializationFormat::NK_AUTO_DETECT)) {
         *     printf("Player saved to native format\n");
         * }
         * @endcode
         */
        template<typename T>
        nk_bool SaveToFile(
            const T& obj,
            const nk_char* path,
            NkSerializationFormat format = NkSerializationFormat::NK_AUTO_DETECT,
            nk_bool pretty = true,
            NkString* outError = nullptr
        ) noexcept {
            static_assert(
                std::is_base_of<NkISerializable, T>::value,
                "T must inherit from NkISerializable"
            );

            if (!path) {
                if (outError) {
                    *outError = NkString("Null path");
                }
                return false;
            }

            if (format == NkSerializationFormat::NK_AUTO_DETECT) {
                format = detail::DetectFormatFromExtension(path);
            }

            if (format == NkSerializationFormat::NK_AUTO_DETECT) {
                format = NkSerializationFormat::NK_NATIVE;
            }

            NkArchive archive;

            if (!obj.Serialize(archive)) {
                if (outError) {
                    *outError = NkString("Serialize() failed");
                }
                return false;
            }

            if (detail::IsBinaryFormat(format)) {
                NkVector<nk_uint8> data;

                if (!SerializeArchiveBinary(archive, format, data, outError)) {
                    return false;
                }

                NkFile file;

                if (!file.Open(path, NkFileMode::NK_WRITE_BINARY)) {
                    if (outError) {
                        *outError = NkString("Cannot open file for writing");
                    }
                    return false;
                }

                if (file.Write(data.Data(), data.Size()) != data.Size()) {
                    if (outError) {
                        *outError = NkString("Write error");
                    }
                    file.Close();
                    return false;
                }

                file.Close();
                return true;

            } else {
                NkString text;

                if (!SerializeArchiveText(archive, format, text, pretty, outError)) {
                    return false;
                }

                NkFile file;

                if (!file.Open(path, NkFileMode::NK_WRITE_BINARY)) {
                    if (outError) {
                        *outError = NkString("Cannot open file for writing");
                    }
                    return false;
                }

                if (file.Write(text.CStr(), text.Length()) != text.Length()) {
                    if (outError) {
                        *outError = NkString("Write error");
                    }
                    file.Close();
                    return false;
                }

                file.Close();
                return true;
            }
        }

        /**
         * @brief Charge un objet NkISerializable depuis un fichier
         * @tparam T Type de l'objet a charger (doit heriter de NkISerializable)
         * @param path Chemin du fichier d'entree (doit etre lisible)
         * @param obj Reference vers l'objet a peupler avec les donnees chargees
         * @param format Format d'entree suppose (NK_AUTO_DETECT utilise extension + magic bytes)
         * @param outError Pointeur optionnel vers une chaine pour les messages d'erreur
         * @return nk_bool vrai si le chargement a reussi, faux sinon
         *
         * @note Si format == NK_AUTO_DETECT, l'extension est d'abord testee ;
         *       si indeterminable, les magic bytes du fichier sont analyses
         * @note L'objet obj doit etre constructible par defaut
         * @note En cas d'erreur d'ouverture, de lecture ou de deserialisation,
         *       outError contient un message descriptif si fourni
         *
         * @example
         * @code
         * Player player;  // Sera peuple par le chargement
         *
         * // Chargement avec detection automatique du format
         * if (LoadFromFile("saves/player.nk", player, NkSerializationFormat::NK_AUTO_DETECT)) {
         *     printf("Player loaded: health = %d\n", player.health);
         * } else {
         *     printf("Failed to load player\n");
         * }
         * @endcode
         */
        template<typename T>
        nk_bool LoadFromFile(
            const nk_char* path,
            T& obj,
            NkSerializationFormat format = NkSerializationFormat::NK_AUTO_DETECT,
            NkString* outError = nullptr
        ) noexcept {
            static_assert(
                traits::NkIsBaseOf<NkISerializable, T>::value,
                "T must inherit from NkISerializable"
            );

            if (!path) {
                if (outError) {
                    *outError = NkString("Null path");
                }
                return false;
            }

            if (format == NkSerializationFormat::NK_AUTO_DETECT) {
                format = detail::DetectFormatFromExtension(path);
            }

            NkFile file;

            if (!file.Open(path, NkFileMode::NK_READ)) {
                if (outError) {
                    *outError = NkString("Cannot open file for reading");
                }
                return false;
            }

            nk_size size = file.Size();

            if (size == 0) {
                file.Close();
                if (outError) {
                    *outError = NkString("Empty file");
                }
                return false;
            }

            NkArchive archive;

            if (format == NkSerializationFormat::NK_AUTO_DETECT || detail::IsBinaryFormat(format)) {
                NkVector<nk_uint8> data(size);

                if (file.Read(data.Data(), size) != size) {
                    file.Close();
                    if (outError) {
                        *outError = NkString("Read error");
                    }
                    return false;
                }

                file.Close();

                if (format == NkSerializationFormat::NK_AUTO_DETECT) {
                    format = detail::DetectFormatFromMagic(data.Data(), size);
                }

                if (detail::IsBinaryFormat(format)) {
                    if (!DeserializeArchiveBinary(data.Data(), size, format, archive, outError)) {
                        return false;
                    }
                } else {
                    NkString text(
                        reinterpret_cast<const nk_char*>(data.Data()),
                        size
                    );
                    if (!DeserializeArchiveText(text.View(), format, archive, outError)) {
                        return false;
                    }
                }

            } else {
                NkString text;
                text.Resize(size);

                if (file.Read(text.Data(), size) != size) {
                    file.Close();
                    if (outError) {
                        *outError = NkString("Read error");
                    }
                    return false;
                }

                file.Close();

                if (!DeserializeArchiveText(text.View(), format, archive, outError)) {
                    return false;
                }
            }

            return obj.Deserialize(archive);
        }

        // =====================================================================
        // FACTORY POLYMORPHIQUE : CREATION DYNAMIQUE D'OBJETS SERIALISABLES
        // =====================================================================
        /**
         * @brief Cree une instance polymorphique d'un objet NkISerializable depuis une archive
         * @param archive Reference constante vers l'archive contenant les donnees
         * @return NkUniquePtr<NkISerializable> vers l'objet cree ou nullptr en cas d'echec
         *
         * @note Cette fonction suppose que l'archive contient un champ "__type__"
         *       identifiant le type concret a instancier via NkSerializableRegistry
         * @note L'objet est deserialise automatiquement apres creation ; si
         *       Deserialize() echoue, le pointeur est detruit et nullptr retourne
         * @warning Cette fonction utilise NkUniquePtr : si votre projet evite
         *          la STL runtime, envisagez une alternative avec NkUniquePtr ou
         *          un allocateur personnalise
         *
         * @example
         * @code
         * // Archive chargee precedent contenant {"__type__": "Player", "health": 75}
         * NkArchive archive;
         * // ... chargement de l'archive ...
         *
         * auto obj = CreateFromArchive(archive);
         * if (obj) {
         *     // obj est un unique_ptr vers un Player (ou autre sous-type)
         *     Player* player = dynamic_cast<Player*>(obj.get());
         *     if (player) {
         *         printf("Loaded player with health: %d\n", player->health);
         *     }
         * }
         * @endcode
         */
        inline NkUniquePtr<NkISerializable>
        CreateFromArchive(const NkArchive& archive) noexcept {
            NkString typeName;

            if (!archive.GetString("__type__", typeName)) {
                return nullptr;
            }

            auto obj = NkSerializableRegistry::Global().Create(typeName.CStr());

            if (obj && !obj->Deserialize(archive)) {
                return nullptr;
            }

            return obj;
        }

    } // namespace nkentseu

#endif // NKENTSEU_SERIALIZATION_NKSERIALIZER_H

// =============================================================================
// EXEMPLES D'UTILISATION - NkSerializer.h
// =============================================================================
//
// -----------------------------------------------------------------------------
// Exemple 1 : Classe serialisable basique avec sauvegarde JSON
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NkSerializer.h>
    #include <NKCore/Log/NkLog.h>

    namespace game {

        class Player : public nkentseu::NkISerializable {
        public:
            nkentseu::nk_int32 health = 100;
            nkentseu::nk_float32 experience = 0.0f;
            nkentseu::nk_char name[64] = "Hero";

            nk_bool Serialize(nkentseu::NkArchive& ar) override {
                ar.SetInt("health", health);
                ar.SetFloat("experience", experience);
                ar.SetString("name", name);
                return true;
            }

            nk_bool Deserialize(const nkentseu::NkArchive& ar) override {
                return ar.GetInt("health", health) &&
                       ar.GetFloat("experience", experience) &&
                       ar.GetString("name", name, sizeof(name));
            }
        };

    } // namespace game

    void SavePlayerExample() {
        game::Player player;
        player.health = 85;
        player.experience = 1250.5f;
        strcpy(player.name, "Aria");

        nkentseu::NkString jsonOutput;

        if (nkentseu::Serialize(
                player,
                nkentseu::NkSerializationFormat::NK_JSON,
                jsonOutput,
                true)) {
            NK_FOUNDATION_LOG_INFO("Serialized player:\n%s", jsonOutput.CStr());
        }

        // Sauvegarde directe dans un fichier
        if (nkentseu::SaveToFile(player, "saves/aria.json",
                nkentseu::NkSerializationFormat::NK_AUTO_DETECT, true)) {
            NK_FOUNDATION_LOG_INFO("Player saved to saves/aria.json");
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Chargement avec detection automatique du format
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NkSerializer.h>

    void LoadPlayerExample() {
        game::Player player;  // Constructeur par defaut requis

        // Detection automatique : .json → JSON, .nk → format natif, etc.
        if (nkentseu::LoadFromFile(
                "saves/aria.json",
                player,
                nkentseu::NkSerializationFormat::NK_AUTO_DETECT)) {
            printf("Loaded player '%s' with health %d\n",
                player.name, player.health);
        } else {
            printf("Failed to load player\n");
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Serialisation binaire pour les sauvegardes compactes
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NkSerializer.h>

    void BinarySaveExample() {
        game::Player player;
        player.health = 50;

        nkentseu::NkVector<nkentseu::nk_uint8> binaryData;

        // Serialisation en format natif NK (binaire optimise)
        if (nkentseu::SerializeBinary(
                player,
                nkentseu::NkSerializationFormat::NK_NATIVE,
                binaryData)) {
            printf("Serialized to %zu bytes (binary)\n", binaryData.Size());

            // Ecriture manuelle si besoin de controle fin
            nkentseu::NkFile file;
            if (file.Open("saves/quick.nk", nkentseu::NkFileMode::NK_WRITE_BINARY)) {
                file.Write(binaryData.Data(), binaryData.Size());
                file.Close();
            }
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Gestion d'erreurs detaillee
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NkSerializer.h>
    #include <NKCore/Log/NkLog.h>

    nk_bool SafeLoadPlayer(const nk_char* path, game::Player& outPlayer) {
        nkentseu::NkString error;

        if (!nkentseu::LoadFromFile(
                path,
                outPlayer,
                nkentseu::NkSerializationFormat::NK_AUTO_DETECT,
                &error)) {
            NK_FOUNDATION_LOG_ERROR("Load failed: %s", error.CStr());
            return false;
        }

        NK_FOUNDATION_LOG_INFO("Player loaded successfully");
        return true;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Factory polymorphique pour chargement dynamique
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NkSerializer.h>

    // Dans un fichier .cpp, enregistrement des types pour la factory
    namespace {
        struct PlayerRegistrar {
            PlayerRegistrar() {
                nkentseu::NkSerializableRegistry::Global().Register<game::Player>("Player");
            }
        };
        static PlayerRegistrar g_playerRegistrar;
    }

    nkentseu::std::unique_ptr<nkentseu::NkISerializable>
    LoadAnyEntity(const nk_char* path) {
        nkentseu::NkArchive archive;

        if (!nkentseu::LoadFromFile(path, archive,
                nkentseu::NkSerializationFormat::NK_AUTO_DETECT)) {
            return nullptr;
        }

        // Creation polymorphique via le champ "__type__" dans l'archive
        return nkentseu::CreateFromArchive(archive);
    }

    void UsageExample() {
        auto entity = LoadAnyEntity("saves/entity.nk");
        if (entity) {
            // L'objet est du type enregistre sous "__type__" dans le fichier
            // Utilisation via l'interface NkISerializable ou dynamic_cast
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Conversion entre formats (JSON → Binaire)
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NkSerializer.h>

    nk_bool ConvertFormat(
        const nk_char* inputPath,
        const nk_char* outputPath,
        nkentseu::NkSerializationFormat outputFormat) {

        // Lecture de l'archive depuis le fichier d'entree (format auto-detecte)
        nkentseu::NkArchive archive;
        if (!nkentseu::LoadFromFile(inputPath, archive,
                nkentseu::NkSerializationFormat::NK_AUTO_DETECT)) {
            return false;
        }

        // Ecriture vers le fichier de sortie dans le format cible
        return nkentseu::SaveToFile(archive, outputPath, outputFormat);
    }

    void ConvertExample() {
        // Convertir un fichier JSON en format binaire natif
        if (ConvertFormat(
                "config.json",
                "config.nk",
                nkentseu::NkSerializationFormat::NK_NATIVE)) {
            printf("Conversion successful\n");
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Validation des donnees avant serialisation
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NkSerializer.h>

    nk_bool ValidateAndSave(const game::Player& player, const nk_char* path) {
        // Validation metier avant serialisation
        if (player.health < 0 || player.health > 100) {
            NK_FOUNDATION_LOG_ERROR("Invalid health value: %d", player.health);
            return false;
        }

        if (player.experience < 0) {
            NK_FOUNDATION_LOG_ERROR("Invalid experience value: %.2f", player.experience);
            return false;
        }

        // Serialisation si validation reussie
        return nkentseu::SaveToFile(
            player,
            path,
            nkentseu::NkSerializationFormat::NK_AUTO_DETECT,
            true);
    }
*/

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================