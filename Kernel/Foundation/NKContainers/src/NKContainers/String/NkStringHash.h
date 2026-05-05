// -----------------------------------------------------------------------------
// FICHIER: NKContainers/String/NkStringHash.h
// DESCRIPTION: Déclaration des fonctions de hachage pour les chaînes de caractères
//              Ce module fournit une collection d'algorithmes de hachage optimisés
//              pour différents cas d'usage : tables de hachage, empreintes rapides,
//              hachage sensible à la casse, et hachage à la compilation.
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#include <cstdio>
#include <stddef.h>

#ifndef NK_CONTAINERS_STRING_NKSTRINGHASH_H
    #define NK_CONTAINERS_STRING_NKSTRINGHASH_H

    // ---------------------------------------------------------------------
    // INCLUSIONS DES DÉPENDANCES
    // ---------------------------------------------------------------------
    #include "NkStringView.h"
    #include "NKCore/NkTypes.h"
    #include "NKMemory/NkFunction.h"

    // ---------------------------------------------------------------------
    // ESPACE DE NOMS PRINCIPAL
    // ---------------------------------------------------------------------
    namespace nkentseu
    {
        // -----------------------------------------------------------------
        // SOUS-ESPACE DE NOMS : string
        // -----------------------------------------------------------------
        namespace string
        {
            // =================================================================
            // SECTION 1 : ALGORITHMES DE HACHAGE
            // =================================================================

            // -----------------------------------------------------------------
            // FNV-1a (Fowler-Noll-Vo) - 32 bits
            // -----------------------------------------------------------------
            /**
             * @brief Calcule l'empreinte FNV-1a sur 32 bits pour une vue de chaîne.
             * @param str Vue de chaîne à hacher.
             * @return Valeur de hachage 32 bits.
             * @note Algorithme rapide avec bonne distribution, adapté aux tables de hachage.
             */
            NKENTSEU_CONTAINERS_API uint32 NkHashFNV1a32(NkStringView str) NKENTSEU_NOEXCEPT;

            /**
             * @brief Calcule l'empreinte FNV-1a sur 32 bits pour un pointeur C-string.
             * @param str Pointeur vers une chaîne terminée par '\0'.
             * @return Valeur de hachage 32 bits.
             */
            NKENTSEU_CONTAINERS_API uint32 NkHashFNV1a32(const char* str) NKENTSEU_NOEXCEPT;

            /**
             * @brief Calcule l'empreinte FNV-1a sur 32 bits avec longueur explicite.
             * @param str Pointeur vers les données à hacher.
             * @param length Nombre d'octets à traiter.
             * @return Valeur de hachage 32 bits.
             * @note Permet de hacher des données binaires ou des sous-chaînes.
             */
            NKENTSEU_CONTAINERS_API uint32 NkHashFNV1a32(const char* str, usize length) NKENTSEU_NOEXCEPT;

            // -----------------------------------------------------------------
            // FNV-1a - 64 bits
            // -----------------------------------------------------------------
            /**
             * @brief Calcule l'empreinte FNV-1a sur 64 bits pour une vue de chaîne.
             * @param str Vue de chaîne à hacher.
             * @return Valeur de hachage 64 bits.
             * @note Version 64 bits recommandée pour réduire les collisions.
             */
            NKENTSEU_CONTAINERS_API uint64 NkHashFNV1a64(NkStringView str) NKENTSEU_NOEXCEPT;

            /**
             * @brief Calcule l'empreinte FNV-1a sur 64 bits pour un pointeur C-string.
             * @param str Pointeur vers une chaîne terminée par '\0'.
             * @return Valeur de hachage 64 bits.
             */
            NKENTSEU_CONTAINERS_API uint64 NkHashFNV1a64(const char* str) NKENTSEU_NOEXCEPT;

            /**
             * @brief Calcule l'empreinte FNV-1a sur 64 bits avec longueur explicite.
             * @param str Pointeur vers les données à hacher.
             * @param length Nombre d'octets à traiter.
             * @return Valeur de hachage 64 bits.
             */
            NKENTSEU_CONTAINERS_API uint64 NkHashFNV1a64(const char* str, usize length) NKENTSEU_NOEXCEPT;

            // -----------------------------------------------------------------
            // MurmurHash3 - 32 bits
            // -----------------------------------------------------------------
            /**
             * @brief Calcule l'empreinte MurmurHash3 sur 32 bits avec graine optionnelle.
             * @param str Vue de chaîne à hacher.
             * @param seed Graine initiale pour le hachage (valeur par défaut : 0).
             * @return Valeur de hachage 32 bits.
             * @note Algorithme non cryptographique très rapide avec excellente distribution.
             */
            NKENTSEU_CONTAINERS_API uint32 NkHashMurmur3_32(NkStringView str, uint32 seed = 0) NKENTSEU_NOEXCEPT;

            /**
             * @brief Calcule l'empreinte MurmurHash3 sur 32 bits avec longueur et graine.
             * @param str Pointeur vers les données à hacher.
             * @param length Nombre d'octets à traiter.
             * @param seed Graine initiale pour le hachage (valeur par défaut : 0).
             * @return Valeur de hachage 32 bits.
             */
            NKENTSEU_CONTAINERS_API uint32 NkHashMurmur3_32(const char* str, usize length, uint32 seed = 0) NKENTSEU_NOEXCEPT;

            // -----------------------------------------------------------------
            // MurmurHash3 - 128 bits
            // -----------------------------------------------------------------
            /**
             * @brief Calcule l'empreinte MurmurHash3 sur 128 bits.
             * @param str Vue de chaîne à hacher.
             * @param out Tableau de 2 uint64 pour stocker le résultat 128 bits.
             * @param seed Graine initiale pour le hachage (valeur par défaut : 0).
             * @note Le résultat est écrit dans le tableau 'out' fourni par l'appelant.
             */
            NKENTSEU_CONTAINERS_API void NkHashMurmur3_128(NkStringView str, uint64 out[2], uint32 seed = 0) NKENTSEU_NOEXCEPT;

            /**
             * @brief Calcule l'empreinte MurmurHash3 sur 128 bits avec longueur explicite.
             * @param str Pointeur vers les données à hacher.
             * @param length Nombre d'octets à traiter.
             * @param out Tableau de 2 uint64 pour stocker le résultat 128 bits.
             * @param seed Graine initiale pour le hachage (valeur par défaut : 0).
             */
            NKENTSEU_CONTAINERS_API void NkHashMurmur3_128(const char* str, usize length, uint64 out[2], uint32 seed = 0) NKENTSEU_NOEXCEPT;

            // -----------------------------------------------------------------
            // CityHash - 64 bits (haute performance)
            // -----------------------------------------------------------------
            /**
             * @brief Calcule l'empreinte CityHash sur 64 bits pour une vue de chaîne.
             * @param str Vue de chaîne à hacher.
             * @return Valeur de hachage 64 bits.
             * @note Algorithme très rapide développé par Google, optimisé pour les courtes chaînes.
             */
            NKENTSEU_CONTAINERS_API uint64 NkHashCity64(NkStringView str) NKENTSEU_NOEXCEPT;

            /**
             * @brief Calcule l'empreinte CityHash sur 64 bits avec longueur explicite.
             * @param str Pointeur vers les données à hacher.
             * @param length Nombre d'octets à traiter.
             * @return Valeur de hachage 64 bits.
             */
            NKENTSEU_CONTAINERS_API uint64 NkHashCity64(const char* str, usize length) NKENTSEU_NOEXCEPT;

            // -----------------------------------------------------------------
            // CityHash - 128 bits
            // -----------------------------------------------------------------
            /**
             * @brief Calcule l'empreinte CityHash sur 128 bits.
             * @param str Vue de chaîne à hacher.
             * @param out Tableau de 2 uint64 pour stocker le résultat 128 bits.
             */
            NKENTSEU_CONTAINERS_API void NkHashCity128(NkStringView str, uint64 out[2]) NKENTSEU_NOEXCEPT;

            /**
             * @brief Calcule l'empreinte CityHash sur 128 bits avec longueur explicite.
             * @param str Pointeur vers les données à hacher.
             * @param length Nombre d'octets à traiter.
             * @param out Tableau de 2 uint64 pour stocker le résultat 128 bits.
             */
            NKENTSEU_CONTAINERS_API void NkHashCity128(const char* str, usize length, uint64 out[2]) NKENTSEU_NOEXCEPT;

            // -----------------------------------------------------------------
            // SDBM Hash (simple et efficace)
            // -----------------------------------------------------------------
            /**
             * @brief Calcule l'empreinte SDBM sur 32 bits pour une vue de chaîne.
             * @param str Vue de chaîne à hacher.
             * @return Valeur de hachage 32 bits.
             * @note Algorithme simple avec bonne distribution, utilisé historiquement dans SDBM.
             */
            NKENTSEU_CONTAINERS_API uint32 NkHashSDBM32(NkStringView str) NKENTSEU_NOEXCEPT;

            /**
             * @brief Calcule l'empreinte SDBM sur 32 bits pour un pointeur C-string.
             * @param str Pointeur vers une chaîne terminée par '\0'.
             * @return Valeur de hachage 32 bits.
             */
            NKENTSEU_CONTAINERS_API uint32 NkHashSDBM32(const char* str) NKENTSEU_NOEXCEPT;

            /**
             * @brief Calcule l'empreinte SDBM sur 32 bits avec longueur explicite.
             * @param str Pointeur vers les données à hacher.
             * @param length Nombre d'octets à traiter.
             * @return Valeur de hachage 32 bits.
             */
            NKENTSEU_CONTAINERS_API uint32 NkHashSDBM32(const char* str, usize length) NKENTSEU_NOEXCEPT;

            // -----------------------------------------------------------------
            // DJB2 Hash (Bernstein) - Versions 32 et 64 bits
            // -----------------------------------------------------------------
            /**
             * @brief Calcule l'empreinte DJB2 sur 32 bits pour une vue de chaîne.
             * @param str Vue de chaîne à hacher.
             * @return Valeur de hachage 32 bits.
             * @note Algorithme de Daniel J. Bernstein, simple et rapide.
             */
            NKENTSEU_CONTAINERS_API uint32 NkHashDJB2_32(NkStringView str) NKENTSEU_NOEXCEPT;

            /**
             * @brief Calcule l'empreinte DJB2 sur 32 bits pour un pointeur C-string.
             * @param str Pointeur vers une chaîne terminée par '\0'.
             * @return Valeur de hachage 32 bits.
             */
            NKENTSEU_CONTAINERS_API uint32 NkHashDJB2_32(const char* str) NKENTSEU_NOEXCEPT;

            /**
             * @brief Calcule l'empreinte DJB2 sur 32 bits avec longueur explicite.
             * @param str Pointeur vers les données à hacher.
             * @param length Nombre d'octets à traiter.
             * @return Valeur de hachage 32 bits.
             */
            NKENTSEU_CONTAINERS_API uint32 NkHashDJB2_32(const char* str, usize length) NKENTSEU_NOEXCEPT;

            /**
             * @brief Calcule l'empreinte DJB2 sur 64 bits pour une vue de chaîne.
             * @param str Vue de chaîne à hacher.
             * @return Valeur de hachage 64 bits.
             * @note Version 64 bits pour réduire les collisions sur de grands ensembles.
             */
            NKENTSEU_CONTAINERS_API uint64 NkHashDJB2_64(NkStringView str) NKENTSEU_NOEXCEPT;

            /**
             * @brief Calcule l'empreinte DJB2 sur 64 bits avec longueur explicite.
             * @param str Pointeur vers les données à hacher.
             * @param length Nombre d'octets à traiter.
             * @return Valeur de hachage 64 bits.
             */
            NKENTSEU_CONTAINERS_API uint64 NkHashDJB2_64(const char* str, usize length) NKENTSEU_NOEXCEPT;

            // -----------------------------------------------------------------
            // CRC32 (Cyclic Redundancy Check)
            // -----------------------------------------------------------------
            /**
             * @brief Calcule l'empreinte CRC32 pour une vue de chaîne.
             * @param str Vue de chaîne à hacher.
             * @return Valeur de hachage 32 bits.
             * @note Algorithme de détection d'erreurs, également utilisable comme hash rapide.
             */
            NKENTSEU_CONTAINERS_API uint32 NkHashCRC32(NkStringView str) NKENTSEU_NOEXCEPT;

            /**
             * @brief Calcule l'empreinte CRC32 pour un pointeur C-string.
             * @param str Pointeur vers une chaîne terminée par '\0'.
             * @return Valeur de hachage 32 bits.
             */
            NKENTSEU_CONTAINERS_API uint32 NkHashCRC32(const char* str) NKENTSEU_NOEXCEPT;

            /**
             * @brief Calcule l'empreinte CRC32 avec longueur explicite.
             * @param str Pointeur vers les données à hacher.
             * @param length Nombre d'octets à traiter.
             * @return Valeur de hachage 32 bits.
             */
            NKENTSEU_CONTAINERS_API uint32 NkHashCRC32(const char* str, usize length) NKENTSEU_NOEXCEPT;

            // -----------------------------------------------------------------
            // Adler-32 Checksum
            // -----------------------------------------------------------------
            /**
             * @brief Calcule la somme de contrôle Adler-32 pour une vue de chaîne.
             * @param str Vue de chaîne à hacher.
             * @return Valeur de checksum 32 bits.
             * @note Plus rapide que CRC32 mais avec une distribution légèrement moins bonne.
             */
            NKENTSEU_CONTAINERS_API uint32 NkHashAdler32(NkStringView str) NKENTSEU_NOEXCEPT;

            /**
             * @brief Calcule la somme de contrôle Adler-32 pour un pointeur C-string.
             * @param str Pointeur vers une chaîne terminée par '\0'.
             * @return Valeur de checksum 32 bits.
             */
            NKENTSEU_CONTAINERS_API uint32 NkHashAdler32(const char* str) NKENTSEU_NOEXCEPT;

            /**
             * @brief Calcule la somme de contrôle Adler-32 avec longueur explicite.
             * @param str Pointeur vers les données à hacher.
             * @param length Nombre d'octets à traiter.
             * @return Valeur de checksum 32 bits.
             */
            NKENTSEU_CONTAINERS_API uint32 NkHashAdler32(const char* str, usize length) NKENTSEU_NOEXCEPT;

            // -----------------------------------------------------------------
            // XXHash - 32 bits (très rapide)
            // -----------------------------------------------------------------
            /**
             * @brief Calcule l'empreinte XXHash sur 32 bits avec graine optionnelle.
             * @param str Vue de chaîne à hacher.
             * @param seed Graine initiale pour le hachage (valeur par défaut : 0).
             * @return Valeur de hachage 32 bits.
             * @note Algorithme extrêmement rapide avec excellente qualité de distribution.
             */
            NKENTSEU_CONTAINERS_API uint32 NkHashXX32(NkStringView str, uint32 seed = 0) NKENTSEU_NOEXCEPT;

            /**
             * @brief Calcule l'empreinte XXHash sur 32 bits avec longueur et graine.
             * @param str Pointeur vers les données à hacher.
             * @param length Nombre d'octets à traiter.
             * @param seed Graine initiale pour le hachage (valeur par défaut : 0).
             * @return Valeur de hachage 32 bits.
             */
            NKENTSEU_CONTAINERS_API uint32 NkHashXX32(const char* str, usize length, uint32 seed = 0) NKENTSEU_NOEXCEPT;

            // -----------------------------------------------------------------
            // XXHash - 64 bits
            // -----------------------------------------------------------------
            /**
             * @brief Calcule l'empreinte XXHash sur 64 bits avec graine optionnelle.
             * @param str Vue de chaîne à hacher.
             * @param seed Graine initiale pour le hachage (valeur par défaut : 0).
             * @return Valeur de hachage 64 bits.
             */
            NKENTSEU_CONTAINERS_API uint64 NkHashXX64(NkStringView str, uint64 seed = 0) NKENTSEU_NOEXCEPT;

            /**
             * @brief Calcule l'empreinte XXHash sur 64 bits avec longueur et graine.
             * @param str Pointeur vers les données à hacher.
             * @param length Nombre d'octets à traiter.
             * @param seed Graine initiale pour le hachage (valeur par défaut : 0).
             * @return Valeur de hachage 64 bits.
             */
            NKENTSEU_CONTAINERS_API uint64 NkHashXX64(const char* str, usize length, uint64 seed = 0) NKENTSEU_NOEXCEPT;

            // -----------------------------------------------------------------
            // Jenkins Hash (one-at-a-time)
            // -----------------------------------------------------------------
            /**
             * @brief Calcule l'empreinte Jenkins one-at-a-time sur 32 bits.
             * @param str Vue de chaîne à hacher.
             * @return Valeur de hachage 32 bits.
             * @note Algorithme simple de Bob Jenkins, robuste pour les petites chaînes.
             */
            NKENTSEU_CONTAINERS_API uint32 NkHashJenkins32(NkStringView str) NKENTSEU_NOEXCEPT;

            /**
             * @brief Calcule l'empreinte Jenkins one-at-a-time pour un pointeur C-string.
             * @param str Pointeur vers une chaîne terminée par '\0'.
             * @return Valeur de hachage 32 bits.
             */
            NKENTSEU_CONTAINERS_API uint32 NkHashJenkins32(const char* str) NKENTSEU_NOEXCEPT;

            /**
             * @brief Calcule l'empreinte Jenkins one-at-a-time avec longueur explicite.
             * @param str Pointeur vers les données à hacher.
             * @param length Nombre d'octets à traiter.
             * @return Valeur de hachage 32 bits.
             */
            NKENTSEU_CONTAINERS_API uint32 NkHashJenkins32(const char* str, usize length) NKENTSEU_NOEXCEPT;

            // -----------------------------------------------------------------
            // Lookup3 Hash (Bob Jenkins)
            // -----------------------------------------------------------------
            /**
             * @brief Calcule l'empreinte Lookup3 avec graine optionnelle.
             * @param str Vue de chaîne à hacher.
             * @param seed Graine initiale pour le hachage (valeur par défaut : 0).
             * @return Valeur de hachage 32 bits.
             * @note Algorithme de Bob Jenkins optimisé pour la vitesse et la distribution.
             */
            NKENTSEU_CONTAINERS_API uint32 NkHashLookup3(NkStringView str, uint32 seed = 0) NKENTSEU_NOEXCEPT;

            /**
             * @brief Calcule l'empreinte Lookup3 avec longueur et graine.
             * @param str Pointeur vers les données à hacher.
             * @param length Nombre d'octets à traiter.
             * @param seed Graine initiale pour le hachage (valeur par défaut : 0).
             * @return Valeur de hachage 32 bits.
             */
            NKENTSEU_CONTAINERS_API uint32 NkHashLookup3(const char* str, usize length, uint32 seed = 0) NKENTSEU_NOEXCEPT;

            // -----------------------------------------------------------------
            // SpookyHash - 128 bits (Bob Jenkins)
            // -----------------------------------------------------------------
            /**
             * @brief Calcule l'empreinte SpookyHash sur 128 bits avec deux graines.
             * @param str Vue de chaîne à hacher.
             * @param out Tableau de 2 uint64 pour stocker le résultat 128 bits.
             * @param seed1 Première graine initiale (valeur par défaut : 0).
             * @param seed2 Deuxième graine initiale (valeur par défaut : 0).
             * @note Algorithme très rapide de Bob Jenkins pour les grandes quantités de données.
             */
            NKENTSEU_CONTAINERS_API void NkHashSpooky128(NkStringView str, uint64 out[2], uint64 seed1 = 0, uint64 seed2 = 0) NKENTSEU_NOEXCEPT;

            /**
             * @brief Calcule l'empreinte SpookyHash sur 128 bits avec longueur et graines.
             * @param str Pointeur vers les données à hacher.
             * @param length Nombre d'octets à traiter.
             * @param out Tableau de 2 uint64 pour stocker le résultat 128 bits.
             * @param seed1 Première graine initiale (valeur par défaut : 0).
             * @param seed2 Deuxième graine initiale (valeur par défaut : 0).
             */
            NKENTSEU_CONTAINERS_API void NkHashSpooky128(const char* str, usize length, uint64 out[2], uint64 seed1 = 0, uint64 seed2 = 0) NKENTSEU_NOEXCEPT;

            // =================================================================
            // SECTION 2 : WRAPPERS ET FONCTIONS DE HACHAGE PAR DÉFAUT
            // =================================================================

            // -----------------------------------------------------------------
            // Fonctions de hachage par défaut (alias vers FNV-1a)
            // -----------------------------------------------------------------
            /**
             * @brief Fonction de hachage par défaut utilisant FNV-1a 64 bits.
             * @param str Vue de chaîne à hacher.
             * @return Valeur de hachage 64 bits.
             * @note Point d'entrée principal pour le hachage de chaînes dans la bibliothèque.
             */
            inline uint64 NkHash(NkStringView str) NKENTSEU_NOEXCEPT
            {
                return NkHashFNV1a64(str);
            }

            /**
             * @brief Fonction de hachage 32 bits par défaut utilisant FNV-1a.
             * @param str Vue de chaîne à hacher.
             * @return Valeur de hachage 32 bits.
             */
            inline uint32 NkHash32(NkStringView str) NKENTSEU_NOEXCEPT
            {
                return NkHashFNV1a32(str);
            }

            /**
             * @brief Fonction de hachage 64 bits par défaut utilisant FNV-1a.
             * @param str Vue de chaîne à hacher.
             * @return Valeur de hachage 64 bits.
             */
            inline uint64 NkHash64(NkStringView str) NKENTSEU_NOEXCEPT
            {
                return NkHashFNV1a64(str);
            }

            // -----------------------------------------------------------------
            // Fonction de combinaison de hachages
            // -----------------------------------------------------------------
            /**
             * @brief Combine deux valeurs de hachage en une seule.
             * @tparam T Type de la valeur de hachage (généralement uint32 ou uint64).
             * @param seed Valeur de hachage accumulée.
             * @param hash Nouvelle valeur de hachage à intégrer.
             * @return Valeur de hachage combinée.
             * @note Formule inspirée de Boost pour une bonne distribution des bits.
             */
            template <typename T>
            inline T NkHashCombine(T seed, const T& hash) NKENTSEU_NOEXCEPT
            {
                return seed ^ (hash + 0x9e3779b9 + (seed << 6) + (seed >> 2));
            }

            /**
             * @brief Combine récursivement plusieurs valeurs de hachage.
             * @tparam T Type de la valeur de hachage.
             * @tparam Args Types des arguments restants.
             * @param seed Valeur de hachage accumulée.
             * @param hash Valeur de hachage courante.
             * @param args Valeurs de hachage restantes.
             * @return Valeur de hachage finale combinée.
             */
            template <typename T, typename... Args>
            inline T NkHashCombine(T seed, const T& hash, Args... args) NKENTSEU_NOEXCEPT
            {
                seed = NkHashCombine(seed, hash);
                return NkHashCombine(seed, args...);
            }

            // =================================================================
            // SECTION 3 : HACHAGES INSENSIBLES À LA CASSE
            // =================================================================

            // -----------------------------------------------------------------
            // Déclarations des fonctions de hachage insensibles à la casse
            // -----------------------------------------------------------------
            /**
             * @brief Calcule un hachage 32 bits insensible à la casse.
             * @param str Vue de chaîne à hacher.
             * @return Valeur de hachage 32 bits.
             * @note Les caractères alphabétiques sont normalisés avant hachage.
             */
            NKENTSEU_CONTAINERS_API uint32 NkHashIgnoreCase32(NkStringView str) NKENTSEU_NOEXCEPT;

            /**
             * @brief Calcule un hachage 64 bits insensible à la casse.
             * @param str Vue de chaîne à hacher.
             * @return Valeur de hachage 64 bits.
             */
            NKENTSEU_CONTAINERS_API uint64 NkHashIgnoreCase64(NkStringView str) NKENTSEU_NOEXCEPT;

            /**
             * @brief Calcule un hachage FNV-1a 32 bits insensible à la casse.
             * @param str Vue de chaîne à hacher.
             * @return Valeur de hachage 32 bits.
             */
            NKENTSEU_CONTAINERS_API uint32 NkHashFNV1aIgnoreCase32(NkStringView str) NKENTSEU_NOEXCEPT;

            /**
             * @brief Calcule un hachage FNV-1a 64 bits insensible à la casse.
             * @param str Vue de chaîne à hacher.
             * @return Valeur de hachage 64 bits.
             */
            NKENTSEU_CONTAINERS_API uint64 NkHashFNV1aIgnoreCase64(NkStringView str) NKENTSEU_NOEXCEPT;

            // -----------------------------------------------------------------
            // Wrappers par défaut pour hachage insensible à la casse
            // -----------------------------------------------------------------
            /**
             * @brief Wrapper 32 bits pour le hachage insensible à la casse.
             * @param str Vue de chaîne à hacher.
             * @return Valeur de hachage 32 bits.
             */
            inline uint32 NkHash32IgnoreCase(NkStringView str) NKENTSEU_NOEXCEPT
            {
                return NkHashIgnoreCase32(str);
            }

            /**
             * @brief Wrapper 64 bits pour le hachage insensible à la casse.
             * @param str Vue de chaîne à hacher.
             * @return Valeur de hachage 64 bits.
             */
            inline uint64 NkHash64IgnoreCase(NkStringView str) NKENTSEU_NOEXCEPT
            {
                return NkHashIgnoreCase64(str);
            }

            // =================================================================
            // SECTION 4 : HACHAGE À LA COMPILATION (CONSTEXPR)
            // =================================================================

            // -----------------------------------------------------------------
            // Fonctions constexpr pour hachage FNV-1a à la compilation
            // -----------------------------------------------------------------
            /**
             * @brief Calcule un hachage FNV-1a 32 bits à la compilation.
             * @param str Pointeur vers une chaîne littérale.
             * @param length Longueur de la chaîne.
             * @return Valeur de hachage 32 bits calculée à la compilation.
             * @note Utilisable dans des contextes constexpr pour des optimisations statiques.
             */
            constexpr uint32 NkHashCompileTime32(const char* str, usize length) NKENTSEU_NOEXCEPT
            {
                uint32 hash = 2166136261u;
                for (usize i = 0; i < length; ++i)
                {
                    hash ^= static_cast<uint32>(str[i]);
                    hash *= 16777619u;
                }
                return hash;
            }

            /**
             * @brief Calcule un hachage FNV-1a 64 bits à la compilation.
             * @param str Pointeur vers une chaîne littérale.
             * @param length Longueur de la chaîne.
             * @return Valeur de hachage 64 bits calculée à la compilation.
             */
            constexpr uint64 NkHashCompileTime64(const char* str, usize length) NKENTSEU_NOEXCEPT
            {
                uint64 hash = 14695981039346656037ULL;
                for (usize i = 0; i < length; ++i)
                {
                    hash ^= static_cast<uint64>(str[i]);
                    hash *= 1099511628211ULL;
                }
                return hash;
            }

            // -----------------------------------------------------------------
            // Littéraux de chaîne pour hachage compile-time (C++11+)
            // -----------------------------------------------------------------
            #if defined(NK_CPP11)
                inline namespace literals
                {
                    /**
                     * @brief Littéral "_hash32" pour hachage 32 bits compile-time.
                     * @param str Chaîne littérale.
                     * @param length Longueur de la chaîne.
                     * @return Valeur de hachage 32 bits constexpr.
                     */
                    constexpr uint32 operator""_hash32(const char* str, size_t length) NKENTSEU_NOEXCEPT
                    {
                        return NkHashCompileTime32(str, static_cast<usize>(length));
                    }

                    /**
                     * @brief Littéral "_hash64" pour hachage 64 bits compile-time.
                     * @param str Chaîne littérale.
                     * @param length Longueur de la chaîne.
                     * @return Valeur de hachage 64 bits constexpr.
                     */
                    constexpr uint64 operator""_hash64(const char* str, size_t length) NKENTSEU_NOEXCEPT
                    {
                        return NkHashCompileTime64(str, static_cast<usize>(length));
                    }

                    /**
                     * @brief Littéral "_hash" alias vers _hash32 pour hachage compile-time.
                     * @param str Chaîne littérale.
                     * @param length Longueur de la chaîne.
                     * @return Valeur de hachage 32 bits constexpr.
                     */
                    constexpr uint32 operator""_hash(const char* str, size_t length) NKENTSEU_NOEXCEPT
                    {
                        return NkHashCompileTime32(str, static_cast<usize>(length));
                    }

                    /**
                     * @brief Littéral "_murmur" (fallback vers FNV car MurmurHash non constexpr).
                     * @param str Chaîne littérale.
                     * @param length Longueur de la chaîne.
                     * @return Valeur de hachage 32 bits constexpr via FNV-1a.
                     * @note MurmurHash ne supportant pas constexpr, cette fonction utilise FNV-1a en fallback.
                     */
                    constexpr uint32 operator""_murmur(const char* str, size_t length) NKENTSEU_NOEXCEPT
                    {
                        return NkHashCompileTime32(str, static_cast<usize>(length));
                    }
                }
            #endif

            // =================================================================
            // SECTION 5 : SUPPORT POUR TABLES DE HACHAGE
            // =================================================================

            // -----------------------------------------------------------------
            // Foncteur de hachage pour NkUnorderedMap
            // -----------------------------------------------------------------
            /**
             * @brief Foncteur de hachage pour utilisation avec NkUnorderedMap.
             * @note Compatible avec la STL et les conteneurs associatifs.
             */
            struct NKENTSEU_CONTAINERS_API NkStringHash
            {
                /**
                 * @brief Opérateur de hachage pour NkStringView.
                 * @param str Vue de chaîne à hacher.
                 * @return Valeur de hachage de type usize.
                 */
                usize operator()(NkStringView str) const NKENTSEU_NOEXCEPT
                {
                    return static_cast<usize>(NkHash64(str));
                }

                /**
                 * @brief Opérateur de hachage pour C-string.
                 * @param str Pointeur vers une chaîne terminée par '\0'.
                 * @return Valeur de hachage de type usize.
                 */
                usize operator()(const char* str) const NKENTSEU_NOEXCEPT
                {
                    return static_cast<usize>(NkHashFNV1a64(str));
                }
            };

            // -----------------------------------------------------------------
            // Foncteur de hachage insensible à la casse
            // -----------------------------------------------------------------
            /**
             * @brief Foncteur de hachage insensible à la casse pour NkUnorderedMap.
             */
            struct NKENTSEU_CONTAINERS_API NkStringHashIgnoreCase
            {
                /**
                 * @brief Opérateur de hachage insensible à la casse pour NkStringView.
                 * @param str Vue de chaîne à hacher.
                 * @return Valeur de hachage de type usize.
                 */
                usize operator()(NkStringView str) const NKENTSEU_NOEXCEPT
                {
                    return static_cast<usize>(NkHash64IgnoreCase(str));
                }

                /**
                 * @brief Opérateur de hachage insensible à la casse pour C-string.
                 * @param str Pointeur vers une chaîne terminée par '\0'.
                 * @return Valeur de hachage de type usize.
                 * @note Gère les pointeurs nullptr en retournant 0.
                 */
                usize operator()(const char* str) const NKENTSEU_NOEXCEPT
                {
                    if (!str)
                    {
                        return 0;
                    }
                    return static_cast<usize>(NkHashIgnoreCase64(NkStringView(str)));
                }
            };

            // -----------------------------------------------------------------
            // Foncteur de hachage avec graine configurable
            // -----------------------------------------------------------------
            /**
             * @brief Foncteur de hachage paramétrable avec graine.
             * @tparam SeedType Type de la graine (uint32 ou uint64).
             * @note Sélectionne automatiquement MurmurHash3 ou XXHash selon la taille de la graine.
             */
            template <typename SeedType = uint32>
            struct NKENTSEU_CONTAINERS_API NkSeededStringHash
            {
                SeedType seed;

                /**
                 * @brief Constructeur avec initialisation de la graine.
                 * @param seed_ Valeur initiale de la graine.
                 */
                explicit NkSeededStringHash(SeedType seed_ = 0) : seed(seed_)
                {
                }

                /**
                 * @brief Opérateur de hachage pour NkStringView.
                 * @param str Vue de chaîne à hacher.
                 * @return Valeur de hachage de type usize.
                 */
                usize operator()(NkStringView str) const NKENTSEU_NOEXCEPT
                {
                    if constexpr (sizeof(SeedType) <= 4)
                    {
                        return static_cast<usize>(NkHashMurmur3_32(str, static_cast<uint32>(seed)));
                    }
                    else
                    {
                        return static_cast<usize>(NkHashXX64(str, static_cast<uint64>(seed)));
                    }
                }

                /**
                 * @brief Opérateur de hachage pour C-string.
                 * @param str Pointeur vers une chaîne terminée par '\0'.
                 * @return Valeur de hachage de type usize.
                 * @note Gère les pointeurs nullptr en retournant la graine.
                 */
                usize operator()(const char* str) const NKENTSEU_NOEXCEPT
                {
                    if (!str)
                    {
                        return static_cast<usize>(seed);
                    }
                    return operator()(NkStringView(str));
                }
            };

            // =================================================================
            // SECTION 6 : COMPARATEURS DE HACHAGE
            // =================================================================

            // -----------------------------------------------------------------
            // Comparateur d'égalité pour hachages
            // -----------------------------------------------------------------
            /**
             * @brief Comparateur d'égalité pour utilisation avec les foncteurs de hachage.
             * @tparam HashType Type du foncteur de hachage associé.
             */
            template <typename HashType = NkStringHash>
            struct NKENTSEU_CONTAINERS_API NkStringHashEqual
            {
                /**
                 * @brief Compare deux NkStringView pour l'égalité.
                 * @param lhs Première vue de chaîne.
                 * @param rhs Deuxième vue de chaîne.
                 * @return true si les chaînes sont identiques, false sinon.
                 */
                bool operator()(NkStringView lhs, NkStringView rhs) const NKENTSEU_NOEXCEPT
                {
                    return lhs == rhs;
                }

                /**
                 * @brief Compare deux C-strings pour l'égalité.
                 * @param lhs Premier pointeur de chaîne.
                 * @param rhs Deuxième pointeur de chaîne.
                 * @return true si les chaînes sont identiques, false sinon.
                 */
                bool operator()(const char* lhs, const char* rhs) const NKENTSEU_NOEXCEPT
                {
                    return NkStringView(lhs) == NkStringView(rhs);
                }
            };

            // -----------------------------------------------------------------
            // Comparateur d'égalité insensible à la casse
            // -----------------------------------------------------------------
            /**
             * @brief Comparateur d'égalité insensible à la casse.
             * @tparam HashType Type du foncteur de hachage associé.
             */
            template <typename HashType = NkStringHashIgnoreCase>
            struct NKENTSEU_CONTAINERS_API NkStringHashEqualIgnoreCase
            {
                /**
                 * @brief Compare deux NkStringView sans tenir compte de la casse.
                 * @param lhs Première vue de chaîne.
                 * @param rhs Deuxième vue de chaîne.
                 * @return true si les chaînes sont égales (ignoring case), false sinon.
                 */
                bool operator()(NkStringView lhs, NkStringView rhs) const NKENTSEU_NOEXCEPT
                {
                    return lhs.EqualsIgnoreCase(rhs);
                }

                /**
                 * @brief Compare deux C-strings sans tenir compte de la casse.
                 * @param lhs Premier pointeur de chaîne.
                 * @param rhs Deuxième pointeur de chaîne.
                 * @return true si les chaînes sont égales (ignoring case), false sinon.
                 */
                bool operator()(const char* lhs, const char* rhs) const NKENTSEU_NOEXCEPT
                {
                    return NkStringView(lhs).EqualsIgnoreCase(NkStringView(rhs));
                }
            };

            // =================================================================
            // SECTION 7 : FONCTIONS UTILITAIRES
            // =================================================================

            // -----------------------------------------------------------------
            // Hachage rapide pour petites chaînes
            // -----------------------------------------------------------------
            /**
             * @brief Fonction de hachage optimisée pour les courtes chaînes (32 bits).
             * @param str Vue de chaîne à hacher.
             * @return Valeur de hachage 32 bits.
             * @note Utilise un algorithme simplifié pour maximiser la vitesse sur < 32 octets.
             */
            NKENTSEU_CONTAINERS_API uint32 NkHashFast32(NkStringView str) NKENTSEU_NOEXCEPT;

            /**
             * @brief Fonction de hachage optimisée pour les courtes chaînes (64 bits).
             * @param str Vue de chaîne à hacher.
             * @return Valeur de hachage 64 bits.
             */
            NKENTSEU_CONTAINERS_API uint64 NkHashFast64(NkStringView str) NKENTSEU_NOEXCEPT;

            // -----------------------------------------------------------------
            // Hachage cryptographique simplifié (SHA-1)
            // -----------------------------------------------------------------
            /**
             * @brief Calcule une empreinte SHA-1 simplifiée (non sécurisée).
             * @param str Vue de chaîne à hacher.
             * @param out Tableau de 20 uint8 pour stocker le résultat.
             * @note À usage interne uniquement, ne pas utiliser pour la sécurité.
             */
            NKENTSEU_CONTAINERS_API void NkHashSHA1(NkStringView str, uint8 out[20]) NKENTSEU_NOEXCEPT;

            /**
             * @brief Calcule une empreinte SHA-1 simplifiée avec longueur explicite.
             * @param str Pointeur vers les données à hacher.
             * @param length Nombre d'octets à traiter.
             * @param out Tableau de 20 uint8 pour stocker le résultat.
             */
            NKENTSEU_CONTAINERS_API void NkHashSHA1(const char* str, usize length, uint8 out[20]) NKENTSEU_NOEXCEPT;

            // -----------------------------------------------------------------
            // Conversion de hachage vers représentations textuelles
            // -----------------------------------------------------------------
            /**
             * @brief Convertit un hachage 32 bits en chaîne hexadécimale.
             * @param hash Valeur de hachage 32 bits.
             * @return Chaîne hexadécimale représentant le hachage.
             */
            NKENTSEU_CONTAINERS_API NkString NkHashToHex32(uint32 hash) NKENTSEU_NOEXCEPT;

            /**
             * @brief Convertit un hachage 64 bits en chaîne hexadécimale.
             * @param hash Valeur de hachage 64 bits.
             * @return Chaîne hexadécimale représentant le hachage.
             */
            NKENTSEU_CONTAINERS_API NkString NkHashToHex64(uint64 hash) NKENTSEU_NOEXCEPT;

            /**
             * @brief Convertit un hachage 128 bits en chaîne hexadécimale.
             * @param hash Tableau de 2 uint64 contenant le hachage 128 bits.
             * @return Chaîne hexadécimale représentant le hachage.
             */
            NKENTSEU_CONTAINERS_API NkString NkHashToHex128(const uint64 hash[2]) NKENTSEU_NOEXCEPT;

            /**
             * @brief Encode un hachage 32 bits en chaîne Base64.
             * @param hash Valeur de hachage 32 bits.
             * @return Chaîne Base64 représentant le hachage.
             */
            NKENTSEU_CONTAINERS_API NkString NkHashToBase64_32(uint32 hash) NKENTSEU_NOEXCEPT;

            /**
             * @brief Encode un hachage 64 bits en chaîne Base64.
             * @param hash Valeur de hachage 64 bits.
             * @return Chaîne Base64 représentant le hachage.
             */
            NKENTSEU_CONTAINERS_API NkString NkHashToBase64_64(uint64 hash) NKENTSEU_NOEXCEPT;

            // -----------------------------------------------------------------
            // Hachage combiné de multiples chaînes
            // -----------------------------------------------------------------
            /**
             * @brief Combine le hachage de deux chaînes en une valeur 64 bits.
             * @param str1 Première vue de chaîne.
             * @param str2 Deuxième vue de chaîne.
             * @return Valeur de hachage combinée 64 bits.
             */
            NKENTSEU_CONTAINERS_API uint64 NkHashMulti(NkStringView str1, NkStringView str2) NKENTSEU_NOEXCEPT;

            /**
             * @brief Combine le hachage de trois chaînes en une valeur 64 bits.
             * @param str1 Première vue de chaîne.
             * @param str2 Deuxième vue de chaîne.
             * @param str3 Troisième vue de chaîne.
             * @return Valeur de hachage combinée 64 bits.
             */
            NKENTSEU_CONTAINERS_API uint64 NkHashMulti(NkStringView str1, NkStringView str2, NkStringView str3) NKENTSEU_NOEXCEPT;

            /**
             * @brief Combine le hachage de N chaînes via variadic templates.
             * @tparam Args Types des arguments restants.
             * @param first Première vue de chaîne.
             * @param args Vues de chaîne supplémentaires.
             * @return Valeur de hachage combinée 64 bits.
             */
            template <typename... Args>
            inline uint64 NkHashMulti(NkStringView first, Args... args) NKENTSEU_NOEXCEPT
            {
                uint64 hash = NkHash64(first);
                return NkHashCombine(hash, NkHash64(args)...);
            }

            // -----------------------------------------------------------------
            // Outils de benchmark et analyse statistique
            // -----------------------------------------------------------------
            /**
             * @brief Exécute un benchmark de performance sur les algorithmes de hachage.
             * @param testString Chaîne de test pour le benchmark.
             * @param iterations Nombre d'itérations à exécuter (défaut : 1 000 000).
             * @note Affiche les temps d'exécution via stderr ou système de logging.
             */
            NKENTSEU_CONTAINERS_API void NkHashBenchmark(NkStringView testString, usize iterations = 1000000) NKENTSEU_NOEXCEPT;

            /**
             * @brief Estime la probabilité de collision pour un espace de hachage donné.
             * @param numItems Nombre d'éléments à insérer.
             * @param hashSpace Taille de l'espace de hachage (2^bits).
             * @return Probabilité estimée de collision (0.0 à 1.0).
             * @note Basé sur le paradoxe des anniversaires pour estimation statistique.
             */
            NKENTSEU_CONTAINERS_API double NkHashCollisionProbability(usize numItems, usize hashSpace) NKENTSEU_NOEXCEPT;

        } // namespace string

    } // namespace nkentseu

#endif // NK_CONTAINERS_STRING_NKSTRINGHASH_H

// =============================================================================
// EXEMPLES D'UTILISATION
// =============================================================================
/*
    // ---------------------------------------------------------------------
    // Exemple 1 : Hachage simple d'une chaîne
    // ---------------------------------------------------------------------
    #include "NKContainers/String/NkStringHash.h"
    using namespace nkentseu::string;

    NkStringView myStr = "Hello World";
    uint64 hash = NkHash(myStr);  // Utilise FNV-1a 64-bit par défaut

    // ---------------------------------------------------------------------
    // Exemple 2 : Hachage avec algorithme spécifique
    // ---------------------------------------------------------------------
    uint32 murmurHash = NkHashMurmur3_32(myStr, 42);  // Avec graine = 42
    uint64 cityHash = NkHashCity64(myStr);             // Très rapide

    // ---------------------------------------------------------------------
    // Exemple 3 : Hachage insensible à la casse
    // ---------------------------------------------------------------------
    NkStringView str1 = "Test";
    NkStringView str2 = "test";
    uint64 h1 = NkHash64IgnoreCase(str1);
    uint64 h2 = NkHash64IgnoreCase(str2);
    // h1 == h2 -> true

    // ---------------------------------------------------------------------
    // Exemple 4 : Hachage à la compilation avec littéraux
    // ---------------------------------------------------------------------
    #if defined(NK_CPP11)
    using namespace nkentseu::string::literals;
    constexpr uint32 compileHash = "mon_id"_hash32;  // Calculé à la compilation
    #endif

    // ---------------------------------------------------------------------
    // Exemple 5 : Utilisation avec NkUnorderedMap
    // ---------------------------------------------------------------------
    NkUnorderedMap<NkString, int, NkStringHash> myMap;
    myMap["clé"] = 42;

    // Avec hachage insensible à la casse :
    NkUnorderedMap<NkString, int, NkStringHashIgnoreCase, NkStringHashEqualIgnoreCase> ciMap;
    ciMap["Clé"] = 100;
    auto val = ciMap["CLÉ"];  // Retourne 100 (case-insensitive)

    // ---------------------------------------------------------------------
    // Exemple 6 : Combinaison de plusieurs hachages
    // ---------------------------------------------------------------------
    uint64 combined = NkHashCombine(NkHash64("part1"), NkHash64("part2"), NkHash64("part3"));

    // ---------------------------------------------------------------------
    // Exemple 7 : Hachage avec graine personnalisée
    // ---------------------------------------------------------------------
    NkSeededStringHash<uint64> seededHasher(0xDEADBEEF);
    usize result = seededHasher("données sensibles");

    // ---------------------------------------------------------------------
    // Exemple 8 : Conversion en format lisible
    // ---------------------------------------------------------------------
    uint32 hash32 = NkHash32("exemple");
    NkString hex = NkHashToHex32(hash32);  // Ex: "a1b2c3d4"

    // ---------------------------------------------------------------------
    // Exemple 9 : Benchmark de performance
    // ---------------------------------------------------------------------
    NkHashBenchmark("chaîne de test", 500000);  // Affiche les performances

    // ---------------------------------------------------------------------
    // Exemple 10 : Estimation de collisions
    // ---------------------------------------------------------------------
    double proba = NkHashCollisionProbability(10000, 1ULL << 32);
    // proba ≈ probabilité de collision avec 10k éléments sur espace 32-bit
*/

// ============================================================
// Copyright © 2024-2026 Rihen. Tous droits réservés.
// Licence Propriétaire - Utilisation et modification autorisées
//
// Généré par Rihen le 2026-02-05 22:26:13
// Date de création : 2026-02-05 22:26:13
// Dernière modification : 2026-04-25
// ============================================================