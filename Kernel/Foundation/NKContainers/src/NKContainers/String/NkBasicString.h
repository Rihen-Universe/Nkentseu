// -----------------------------------------------------------------------------
// FICHIER: NKContainers/String/NkBasicString.h
// DESCRIPTION: Chaîne générique avec optimisation SSO et support multi-encodage
//              Implémentation inline pour performance et flexibilité template
// AUTEUR: Rihen
// DATE: 2026-02-08
// VERSION: 1.0.1
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CONTAINERS_STRING_NKBASICSTRING_H
    #define NK_CONTAINERS_STRING_NKBASICSTRING_H

    // -------------------------------------------------------------------------
    // INCLUSIONS DES DÉPENDANCES DU PROJET
    // -------------------------------------------------------------------------
    #include "NKCore/NkTypes.h"
    #include "NKCore/Assert/NkAssert.h"
    #include "NKMemory/NkAllocator.h"
    #include "NKMemory/NkFunction.h"
    #include "Encoding/NkEncoding.h"

    // -------------------------------------------------------------------------
    // NAMESPACE PRINCIPAL DU PROJET
    // -------------------------------------------------------------------------
    namespace nkentseu
    {
        // =====================================================================
        // SECTION 1 : TRAITS DE CARACTÈRES (NkCharTraits)
        // =====================================================================

        /**
         * @brief Traits génériques pour opérations sur caractères
         * 
         * Cette structure fournit des opérations de base sur les caractères
         * (longueur, comparaison, copie, etc.) de manière générique via template.
         * Permet d'adapter NkBasicString à différents types de caractères.
         * 
         * @tparam CharT Type de caractère (char, char16_t, char32_t, wchar_t)
         * 
         * @note Toutes les méthodes sont inline pour permettre l'optimisation
         *       par le compilateur lors de l'instanciation du template.
         */
        template <typename CharT>
        struct NkCharTraits
        {
            // -----------------------------------------------------------------
            // TYPES ET CONSTANTES
            // -----------------------------------------------------------------

            /**
             * @brief Alias du type de caractère géré
             */
            using CharType = CharT;

            // -----------------------------------------------------------------
            // MÉTHODES STATIQUES D'OPÉRATIONS SUR CARACTÈRES
            // -----------------------------------------------------------------

            /**
             * @brief Retourne le caractère nul terminal pour ce type
             * @return CharT(0) — le caractère de fin de chaîne
             * @note constexpr pour évaluation à la compilation
             */
            static constexpr CharT Null()
            {
                return CharT(0);
            }

            /**
             * @brief Calcule la longueur d'une C-string terminée par nul
             * @param str Pointeur vers la chaîne à mesurer
             * @return Nombre de caractères avant le nul terminal, ou 0 si nullptr
             * @note Complexité : O(n) où n = longueur réelle
             */
            static inline usize Length(const CharT* str)
            {
                if (!str)
                {
                    return 0;
                }

                usize len = 0;

                while (str[len] != CharT(0))
                {
                    ++len;
                }

                return len;
            }

            /**
             * @brief Compare deux séquences de caractères sur count éléments
             * @param s1 Pointeur vers la première séquence
             * @param s2 Pointeur vers la deuxième séquence
             * @param count Nombre de caractères à comparer
             * @return -1 si s1 < s2, 0 si égales, +1 si s1 > s2
             * @note Comparaison lexicographique octet-par-octet
             */
            static inline int32 Compare(const CharT* s1, const CharT* s2, usize count)
            {
                for (usize i = 0; i < count; ++i)
                {
                    if (s1[i] < s2[i])
                    {
                        return -1;
                    }

                    if (s1[i] > s2[i])
                    {
                        return 1;
                    }
                }

                return 0;
            }

            /**
             * @brief Copie count caractères de src vers dest
             * @param dest Pointeur vers la destination
             * @param src Pointeur vers la source
             * @param count Nombre de caractères à copier
             * @return Pointeur vers dest pour chaînage
             * @note Copie simple, ne gère pas les chevauchements mémoire
             */
            static inline CharT* Copy(CharT* dest, const CharT* src, usize count)
            {
                for (usize i = 0; i < count; ++i)
                {
                    dest[i] = src[i];
                }

                return dest;
            }

            /**
             * @brief Déplace count caractères de src vers dest (gère chevauchements)
             * @param dest Pointeur vers la destination
             * @param src Pointeur vers la source
             * @param count Nombre de caractères à déplacer
             * @return Pointeur vers dest pour chaînage
             * @note Algorithme sécurisé pour les régions mémoire qui se chevauchent
             */
            static inline CharT* Move(CharT* dest, const CharT* src, usize count)
            {
                if (dest < src)
                {
                    for (usize i = 0; i < count; ++i)
                    {
                        dest[i] = src[i];
                    }
                }
                else if (dest > src)
                {
                    for (usize i = count; i > 0; --i)
                    {
                        dest[i - 1] = src[i - 1];
                    }
                }

                return dest;
            }

            /**
             * @brief Remplit count positions avec le caractère ch
             * @param dest Pointeur vers la zone à remplir
             * @param ch Caractère de remplissage
             * @param count Nombre de positions à remplir
             * @return Pointeur vers dest pour chaînage
             */
            static inline CharT* Fill(CharT* dest, CharT ch, usize count)
            {
                for (usize i = 0; i < count; ++i)
                {
                    dest[i] = ch;
                }

                return dest;
            }

            /**
             * @brief Recherche la première occurrence de ch dans str
             * @param str Pointeur vers la chaîne à parcourir
             * @param count Nombre de caractères à examiner
             * @param ch Caractère à rechercher
             * @return Pointeur vers la première occurrence, ou nullptr si non trouvé
             * @note Recherche linéaire O(n)
             */
            static inline const CharT* Find(const CharT* str, usize count, CharT ch)
            {
                for (usize i = 0; i < count; ++i)
                {
                    if (str[i] == ch)
                    {
                        return str + i;
                    }
                }

                return nullptr;
            }
        };

        // =====================================================================
        // SECTION 2 : CLASSE TEMPLATE NkBasicString
        // =====================================================================

        /**
         * @brief Chaîne de caractères générique avec Small String Optimization (SSO)
         * 
         * Cette classe implémente une chaîne dynamique avec optimisation pour
         * les petites chaînes stockées directement dans l'objet (pas d'allocation).
         * Supporte tous les encodages via le paramètre template CharT.
         * 
         * @tparam CharT Type de caractère (char, char16_t, char32_t, wchar_t)
         * @tparam Traits Classe de traits pour opérations sur CharT (défaut: NkCharTraits)
         * 
         * @par Optimisation SSO :
         * Les chaînes de longueur <= SSO_CAPACITY sont stockées dans un buffer
         * interne, évitant toute allocation dynamique. Au-delà, bascule vers heap.
         * 
         * @note Sécurité : Toutes les méthodes d'accès utilisent NKENTSEU_ASSERT
         *       en mode debug pour détecter les accès hors bornes.
         */
        template <typename CharT, typename Traits = NkCharTraits<CharT>>
        class NkBasicString
        {
        public:
            // -----------------------------------------------------------------
            // TYPES ET CONSTANTES PUBLIQUES
            // -----------------------------------------------------------------

            /**
             * @brief Alias du type de caractère géré par cette instance
             */
            using CharType = CharT;

            /**
             * @brief Alias de la classe de traits associée
             */
            using TraitsType = Traits;

            /**
             * @brief Alias du type utilisé pour les tailles et indices
             */
            using SizeType = usize;

            /**
             * @brief Valeur spéciale indiquant "non trouvé" ou "fin"
             * @note Équivalent à std::string::npos
             */
            static constexpr SizeType npos = static_cast<SizeType>(-1);

            /**
             * @brief Capacité maximale pour le stockage SSO (Small String Optimization)
             * @note Calculée pour occuper exactement l'espace d'un pointeur + 2 size_t
             */
            static constexpr SizeType SSO_CAPACITY =
                (sizeof(CharT*) + sizeof(SizeType) * 2) / sizeof(CharT) - 1;

            // =================================================================
            // SECTION 2.1 : CONSTRUCTEURS
            // =================================================================

            // -----------------------------------------------------------------
            // Constructeur par défaut : chaîne vide avec allocateur par défaut
            // -----------------------------------------------------------------
            /**
             * @brief Construit une chaîne vide utilisant l'allocateur par défaut
             * @note Initialise en mode SSO avec buffer interne
             */
            NkBasicString()
                : mAllocator(&memory::NkGetDefaultAllocator())
                , mLength(0)
                , mCapacity(SSO_CAPACITY)
            {
                mSSOData[0] = Traits::Null();
                SetSSO(true);
            }

            // -----------------------------------------------------------------
            // Constructeur avec allocateur personnalisé
            // -----------------------------------------------------------------
            /**
             * @brief Construit une chaîne vide avec un allocateur spécifique
             * @param allocator Référence vers l'allocateur à utiliser
             * @note Utile pour les environnements avec gestion mémoire custom
             */
            explicit NkBasicString(memory::NkIAllocator& allocator)
                : mAllocator(&allocator)
                , mLength(0)
                , mCapacity(SSO_CAPACITY)
            {
                mSSOData[0] = Traits::Null();
                SetSSO(true);
            }

            // -----------------------------------------------------------------
            // Constructeur depuis C-string terminée par nul
            // -----------------------------------------------------------------
            /**
             * @brief Construit une chaîne depuis une C-string
             * @param str Pointeur vers la chaîne source (peut être nullptr)
             * @note Copie les données, gère nullptr silencieusement
             */
            NkBasicString(const CharT* str)
                : NkBasicString()
            {
                if (str)
                {
                    Append(str);
                }
            }

            // -----------------------------------------------------------------
            // Constructeur depuis pointeur + longueur explicite
            // -----------------------------------------------------------------
            /**
             * @brief Construit une chaîne depuis un pointeur et une longueur
             * @param str Pointeur vers les données caractères
             * @param length Nombre de caractères à copier
             * @note Permet de construire depuis des sous-chaînes ou données binaires
             */
            NkBasicString(const CharT* str, SizeType length)
                : NkBasicString()
            {
                if (str && length > 0)
                {
                    Append(str, length);
                }
            }

            // -----------------------------------------------------------------
            // Constructeur depuis C-string avec allocateur personnalisé
            // -----------------------------------------------------------------
            /**
             * @brief Construit une chaîne depuis C-string avec allocateur custom
             * @param str Pointeur vers la chaîne source
             * @param allocator Référence vers l'allocateur à utiliser
             */
            NkBasicString(const CharT* str, memory::NkIAllocator& allocator)
                : NkBasicString(allocator)
            {
                if (str)
                {
                    Append(str);
                }
            }

            // -----------------------------------------------------------------
            // Constructeur de répétition : count fois le caractère ch
            // -----------------------------------------------------------------
            /**
             * @brief Construit une chaîne contenant count répétitions de ch
             * @param count Nombre de répétitions souhaitées
             * @param ch Caractère à répéter
             * @note Complexité : O(count), allocation si count > SSO_CAPACITY
             */
            NkBasicString(SizeType count, CharT ch)
                : NkBasicString()
            {
                if (count > 0)
                {
                    Append(count, ch);
                }
            }

            // -----------------------------------------------------------------
            // Constructeur de répétition avec allocateur personnalisé
            // -----------------------------------------------------------------
            /**
             * @brief Construit une chaîne répétée avec allocateur custom
             * @param count Nombre de répétitions souhaitées
             * @param ch Caractère à répéter
             * @param allocator Référence vers l'allocateur à utiliser
             */
            NkBasicString(SizeType count, CharT ch, memory::NkIAllocator& allocator)
                : NkBasicString(allocator)
            {
                if (count > 0)
                {
                    Append(count, ch);
                }
            }

            // -----------------------------------------------------------------
            // Constructeur de copie
            // -----------------------------------------------------------------
            /**
             * @brief Construit une chaîne par copie profonde d'une autre
             * @param other Chaîne source à copier
             * @note Copie le contenu et l'allocateur, préserve le mode SSO/heap
             */
            NkBasicString(const NkBasicString& other)
                : mAllocator(other.mAllocator)
                , mLength(other.mLength)
                , mCapacity(other.mCapacity)
            {
                if (other.IsSSO())
                {
                    Traits::Copy(mSSOData, other.mSSOData, mLength + 1);
                    SetSSO(true);
                }
                else
                {
                    AllocateHeap(mLength);
                    Traits::Copy(mHeapData, other.mHeapData, mLength + 1);
                }
            }

            // -----------------------------------------------------------------
            // Constructeur de déplacement (move)
            // -----------------------------------------------------------------
            /**
             * @brief Construit une chaîne par transfert de ressources (move)
             * @param other Chaîne source dont les ressources sont transférées
             * @note Opération noexcept, other devient vide après le transfert
             */
            NkBasicString(NkBasicString&& other) NKENTSEU_NOEXCEPT
                : mAllocator(other.mAllocator)
                , mLength(other.mLength)
                , mCapacity(other.mCapacity)
            {
                if (other.IsSSO())
                {
                    Traits::Copy(mSSOData, other.mSSOData, mLength + 1);
                    SetSSO(true);
                }
                else
                {
                    mHeapData = other.mHeapData;
                    other.mHeapData = nullptr;
                    SetSSO(false);
                }

                other.mLength = 0;
                other.mCapacity = SSO_CAPACITY;
                other.mSSOData[0] = Traits::Null();
                other.SetSSO(true);
            }

            // -----------------------------------------------------------------
            // Destructeur
            // -----------------------------------------------------------------
            /**
             * @brief Libère les ressources heap si nécessaire
             * @note Ne fait rien en mode SSO (buffer interne)
             */
            ~NkBasicString()
            {
                if (!IsSSO())
                {
                    DeallocateHeap();
                }
            }

            // =================================================================
            // SECTION 2.2 : OPÉRATEURS D'ASSIGNATION
            // =================================================================

            // -----------------------------------------------------------------
            // Assignation par copie
            // -----------------------------------------------------------------
            /**
             * @brief Assigne le contenu d'une autre chaîne par copie
             * @param other Chaîne source à copier
             * @return Référence vers this pour chaînage
             * @note Gère l'auto-assignation, réutilise la capacité existante si possible
             */
            NkBasicString& operator=(const NkBasicString& other)
            {
                if (this != &other)
                {
                    Clear();
                    Append(other);
                }

                return *this;
            }

            // -----------------------------------------------------------------
            // Assignation par déplacement (move)
            // -----------------------------------------------------------------
            /**
             * @brief Assigne par transfert de ressources (move)
             * @param other Chaîne source dont les ressources sont transférées
             * @return Référence vers this pour chaînage
             * @note Opération noexcept, other devient vide après transfert
             */
            NkBasicString& operator=(NkBasicString&& other) NKENTSEU_NOEXCEPT
            {
                if (this != &other)
                {
                    if (!IsSSO())
                    {
                        DeallocateHeap();
                    }

                    mLength = other.mLength;
                    mCapacity = other.mCapacity;

                    if (other.IsSSO())
                    {
                        Traits::Copy(mSSOData, other.mSSOData, mLength + 1);
                        SetSSO(true);
                    }
                    else
                    {
                        mHeapData = other.mHeapData;
                        other.mHeapData = nullptr;
                        SetSSO(false);
                    }

                    other.mLength = 0;
                    other.mCapacity = SSO_CAPACITY;
                    other.mSSOData[0] = Traits::Null();
                    other.SetSSO(true);
                }

                return *this;
            }

            // -----------------------------------------------------------------
            // Assignation depuis C-string
            // -----------------------------------------------------------------
            /**
             * @brief Assigne le contenu d'une C-string
             * @param str Pointeur vers la chaîne source
             * @return Référence vers this pour chaînage
             * @note Gère nullptr silencieusement (chaîne vide résultante)
             */
            NkBasicString& operator=(const CharT* str)
            {
                Clear();

                if (str)
                {
                    Append(str);
                }

                return *this;
            }

            // -----------------------------------------------------------------
            // Assignation depuis un caractère unique
            // -----------------------------------------------------------------
            /**
             * @brief Assigne un seul caractère à la chaîne
             * @param ch Caractère à assigner
             * @return Référence vers this pour chaînage
             * @note Équivalent à Clear() puis Append(ch)
             */
            NkBasicString& operator=(CharT ch)
            {
                Clear();
                Append(ch);
                return *this;
            }

            // =================================================================
            // SECTION 2.3 : ACCÈS AUX ÉLÉMENTS
            // =================================================================

            // -----------------------------------------------------------------
            // Opérateur d'indice (non-const)
            // -----------------------------------------------------------------
            /**
             * @brief Accède au caractère à l'index spécifié (avec assertion debug)
             * @param index Position du caractère (0 <= index < Length())
             * @return Référence modifiable vers le caractère
             * @warning NKENTSEU_ASSERT en mode debug si index hors bornes
             */
            CharT& operator[](SizeType index)
            {
                NKENTSEU_ASSERT(index < mLength);
                return GetData()[index];
            }

            // -----------------------------------------------------------------
            // Opérateur d'indice (const)
            // -----------------------------------------------------------------
            /**
             * @brief Accède au caractère à l'index spécifié (version const)
             * @param index Position du caractère (0 <= index < Length())
             * @return Référence constante vers le caractère
             */
            const CharT& operator[](SizeType index) const
            {
                NKENTSEU_ASSERT(index < mLength);
                return GetData()[index];
            }

            // -----------------------------------------------------------------
            // Méthode At (non-const)
            // -----------------------------------------------------------------
            /**
             * @brief Accède au caractère avec vérification de borne explicite
             * @param index Position du caractère
             * @return Référence modifiable vers le caractère
             * @note Identique à operator[] ici, mais sémantiquement plus explicite
             */
            CharT& At(SizeType index)
            {
                NKENTSEU_ASSERT(index < mLength);
                return GetData()[index];
            }

            // -----------------------------------------------------------------
            // Méthode At (const)
            // -----------------------------------------------------------------
            /**
             * @brief Accède au caractère avec vérification (version const)
             * @param index Position du caractère
             * @return Référence constante vers le caractère
             */
            const CharT& At(SizeType index) const
            {
                NKENTSEU_ASSERT(index < mLength);
                return GetData()[index];
            }

            // -----------------------------------------------------------------
            // Accès au premier caractère (non-const)
            // -----------------------------------------------------------------
            /**
             * @brief Retourne une référence modifiable au premier caractère
             * @return Référence vers mData[0]
             * @warning Assertion si la chaîne est vide
             */
            CharT& Front()
            {
                NKENTSEU_ASSERT(mLength > 0);
                return GetData()[0];
            }

            // -----------------------------------------------------------------
            // Accès au premier caractère (const)
            // -----------------------------------------------------------------
            /**
             * @brief Retourne une référence constante au premier caractère
             * @return Référence constante vers mData[0]
             */
            const CharT& Front() const
            {
                NKENTSEU_ASSERT(mLength > 0);
                return GetData()[0];
            }

            // -----------------------------------------------------------------
            // Accès au dernier caractère (non-const)
            // -----------------------------------------------------------------
            /**
             * @brief Retourne une référence modifiable au dernier caractère
             * @return Référence vers mData[Length()-1]
             * @warning Assertion si la chaîne est vide
             */
            CharT& Back()
            {
                NKENTSEU_ASSERT(mLength > 0);
                return GetData()[mLength - 1];
            }

            // -----------------------------------------------------------------
            // Accès au dernier caractère (const)
            // -----------------------------------------------------------------
            /**
             * @brief Retourne une référence constante au dernier caractère
             * @return Référence constante vers mData[Length()-1]
             */
            const CharT& Back() const
            {
                NKENTSEU_ASSERT(mLength > 0);
                return GetData()[mLength - 1];
            }

            // -----------------------------------------------------------------
            // Accès aux données brutes (const)
            // -----------------------------------------------------------------
            /**
             * @brief Retourne un pointeur constant vers les données
             * @return Pointeur vers le premier caractère, toujours nul-terminé
             * @note noexcept, compatible avec les API C attendants const CharT*
             */
            const CharT* Data() const NKENTSEU_NOEXCEPT
            {
                return GetData();
            }

            // -----------------------------------------------------------------
            // Accès aux données brutes (non-const)
            // -----------------------------------------------------------------
            /**
             * @brief Retourne un pointeur modifiable vers les données
             * @return Pointeur vers le premier caractère
             * @warning La modification directe peut invalider l'état interne
             */
            CharT* Data() NKENTSEU_NOEXCEPT
            {
                return GetData();
            }

            // -----------------------------------------------------------------
            // Accès C-string compatible (alias de Data)
            // -----------------------------------------------------------------
            /**
             * @brief Alias de Data() pour compatibilité sémantique C-string
             * @return Pointeur constant vers les données nul-terminées
             * @note Identique à Data(), mais nom plus explicite pour interop C
             */
            const CharT* CStr() const NKENTSEU_NOEXCEPT
            {
                return GetData();
            }

            // =================================================================
            // SECTION 2.4 : CAPACITÉ ET GESTION DE MÉMOIRE
            // =================================================================

            // -----------------------------------------------------------------
            // Longueur actuelle de la chaîne
            // -----------------------------------------------------------------
            /**
             * @brief Retourne le nombre de caractères dans la chaîne
             * @return Valeur de type SizeType
             * @note Complexité : O(1), valeur pré-calculée
             */
            SizeType Length() const NKENTSEU_NOEXCEPT
            {
                return mLength;
            }

            // -----------------------------------------------------------------
            // Taille (alias pour compatibilité STL)
            // -----------------------------------------------------------------
            /**
             * @brief Alias de Length() pour compatibilité avec les conteneurs STL
             * @return Nombre de caractères dans la chaîne
             */
            SizeType Size() const NKENTSEU_NOEXCEPT
            {
                return mLength;
            }

            // -----------------------------------------------------------------
            // Capacité allouée
            // -----------------------------------------------------------------
            /**
             * @brief Retourne le nombre maximal de caractères pouvant être stockés
             * @return Capacité actuelle (SSO_CAPACITY ou capacité heap)
             * @note Ne pas confondre avec Length() — Capacity >= Length toujours
             */
            SizeType Capacity() const NKENTSEU_NOEXCEPT
            {
                return mCapacity;
            }

            // -----------------------------------------------------------------
            // Vérification de vacuité
            // -----------------------------------------------------------------
            /**
             * @brief Indique si la chaîne ne contient aucun caractère
             * @return true si Length() == 0, false sinon
             */
            bool Empty() const NKENTSEU_NOEXCEPT
            {
                return mLength == 0;
            }

            // -----------------------------------------------------------------
            // Pré-allocation de capacité
            // -----------------------------------------------------------------
            /**
             * @brief Réserve de la mémoire pour au moins newCapacity caractères
             * @param newCapacity Nouvelle capacité minimale souhaitée
             * @note Ne réduit jamais la capacité, ignore si newCapacity <= Capacity()
             * @warning Peut déclencher une allocation heap si dépassement SSO
             */
            void Reserve(SizeType newCapacity)
            {
                if (newCapacity <= mCapacity)
                {
                    return;
                }

                CharT* newData = static_cast<CharT*>(
                    mAllocator->Allocate((newCapacity + 1) * sizeof(CharT))
                );

                Traits::Copy(newData, GetData(), mLength + 1);

                if (!IsSSO())
                {
                    mAllocator->Deallocate(mHeapData);
                }

                mHeapData = newData;
                mCapacity = newCapacity;
                SetSSO(false);
            }

            // -----------------------------------------------------------------
            // Réduction de capacité à la taille utile
            // -----------------------------------------------------------------
            /**
             * @brief Réduit la capacité pour correspondre à la longueur actuelle
             * @note Peut basculer de heap vers SSO si Length() <= SSO_CAPACITY
             * @warning Peut déclencher une réallocation et copie des données
             */
            void ShrinkToFit()
            {
                if (IsSSO() || mLength >= mCapacity)
                {
                    return;
                }

                if (mLength <= SSO_CAPACITY)
                {
                    CharT temp[SSO_CAPACITY + 1];
                    Traits::Copy(temp, mHeapData, mLength + 1);
                    DeallocateHeap();
                    Traits::Copy(mSSOData, temp, mLength + 1);
                    mCapacity = SSO_CAPACITY;
                    SetSSO(true);
                }
            }

            // -----------------------------------------------------------------
            // Vidage complet de la chaîne
            // -----------------------------------------------------------------
            /**
             * @brief Réinitialise la chaîne à l'état vide
             * @note Ne libère pas la mémoire allouée (voir ShrinkToFit() pour cela)
             * @warning noexcept, opération très rapide O(1)
             */
            void Clear() NKENTSEU_NOEXCEPT
            {
                mLength = 0;
                GetData()[0] = Traits::Null();
            }

            // -----------------------------------------------------------------
            // Redimensionnement avec caractère de remplissage par défaut
            // -----------------------------------------------------------------
            /**
             * @brief Redimensionne la chaîne, remplit avec le caractère nul si extension
             * @param count Nouvelle longueur souhaitée
             * @note Si count < Length() : tronque ; si count > Length() : étend avec Null()
             */
            void Resize(SizeType count)
            {
                Resize(count, Traits::Null());
            }

            // -----------------------------------------------------------------
            // Redimensionnement avec caractère de remplissage personnalisé
            // -----------------------------------------------------------------
            /**
             * @brief Redimensionne la chaîne avec caractère de remplissage spécifié
             * @param count Nouvelle longueur souhaitée
             * @param ch Caractère à utiliser pour l'extension
             * @note Si count < Length() : tronque ; si count > Length() : étend avec ch
             */
            void Resize(SizeType count, CharT ch)
            {
                if (count < mLength)
                {
                    mLength = count;
                    GetData()[mLength] = Traits::Null();
                }
                else if (count > mLength)
                {
                    GrowIfNeeded(count - mLength);
                    Traits::Fill(GetData() + mLength, ch, count - mLength);
                    mLength = count;
                    GetData()[mLength] = Traits::Null();
                }
            }

            // =================================================================
            // SECTION 2.5 : MODIFICATEURS — AJOUT ET INSERTION
            // =================================================================

            // -----------------------------------------------------------------
            // Ajout depuis C-string terminée par nul
            // -----------------------------------------------------------------
            /**
             * @brief Ajoute le contenu d'une C-string à la fin
             * @param str Pointeur vers la chaîne à ajouter
             * @return Référence vers this pour chaînage
             * @note Gère nullptr silencieusement (aucune modification)
             */
            NkBasicString& Append(const CharT* str)
            {
                if (!str)
                {
                    return *this;
                }

                return Append(str, Traits::Length(str));
            }

            // -----------------------------------------------------------------
            // Ajout depuis pointeur + longueur explicite
            // -----------------------------------------------------------------
            /**
             * @brief Ajoute length caractères depuis str à la fin
             * @param str Pointeur vers les données à ajouter
             * @param length Nombre de caractères à ajouter
             * @return Référence vers this pour chaînage
             * @note Permet d'ajouter des sous-chaînes ou données binaires
             */
            NkBasicString& Append(const CharT* str, SizeType length)
            {
                if (!str || length == 0)
                {
                    return *this;
                }

                GrowIfNeeded(length);
                Traits::Copy(GetData() + mLength, str, length);
                mLength += length;
                GetData()[mLength] = Traits::Null();
                return *this;
            }

            // -----------------------------------------------------------------
            // Ajout depuis une autre NkBasicString
            // -----------------------------------------------------------------
            /**
             * @brief Ajoute le contenu d'une autre chaîne à la fin
             * @param str Chaîne source à concaténer
             * @return Référence vers this pour chaînage
             * @note Délègue à Append(Data(), Length())
             */
            NkBasicString& Append(const NkBasicString& str)
            {
                return Append(str.Data(), str.Length());
            }

            // -----------------------------------------------------------------
            // Ajout de répétitions d'un caractère
            // -----------------------------------------------------------------
            /**
             * @brief Ajoute count répétitions du caractère ch à la fin
             * @param count Nombre de répétitions à ajouter
             * @param ch Caractère à répéter
             * @return Référence vers this pour chaînage
             * @note Utile pour créer du padding ou des séparateurs
             */
            NkBasicString& Append(SizeType count, CharT ch)
            {
                if (count == 0)
                {
                    return *this;
                }

                GrowIfNeeded(count);
                Traits::Fill(GetData() + mLength, ch, count);
                mLength += count;
                GetData()[mLength] = Traits::Null();
                return *this;
            }

            // -----------------------------------------------------------------
            // Ajout d'un caractère unique
            // -----------------------------------------------------------------
            /**
             * @brief Ajoute un seul caractère à la fin de la chaîne
             * @param ch Caractère à ajouter
             * @return Référence vers this pour chaînage
             * @note Équivalent à PushBack(), plus sémantique pour concaténation
             */
            NkBasicString& Append(CharT ch)
            {
                GrowIfNeeded(1);
                GetData()[mLength++] = ch;
                GetData()[mLength] = Traits::Null();
                return *this;
            }

            // -----------------------------------------------------------------
            // Opérateur += pour C-string
            // -----------------------------------------------------------------
            /**
             * @brief Opérateur de concaténation avec C-string
             * @param str Chaîne C à concaténer
             * @return Référence vers this pour chaînage
             * @note Alias vers Append(str)
             */
            NkBasicString& operator+=(const CharT* str)
            {
                return Append(str);
            }

            // -----------------------------------------------------------------
            // Opérateur += pour NkBasicString
            // -----------------------------------------------------------------
            /**
             * @brief Opérateur de concaténation avec NkBasicString
             * @param str Chaîne à concaténer
             * @return Référence vers this pour chaînage
             * @note Alias vers Append(str)
             */
            NkBasicString& operator+=(const NkBasicString& str)
            {
                return Append(str);
            }

            // -----------------------------------------------------------------
            // Opérateur += pour caractère unique
            // -----------------------------------------------------------------
            /**
             * @brief Opérateur de concaténation avec un caractère
             * @param ch Caractère à concaténer
             * @return Référence vers this pour chaînage
             * @note Alias vers Append(ch)
             */
            NkBasicString& operator+=(CharT ch)
            {
                return Append(ch);
            }

            // -----------------------------------------------------------------
            // Insertion depuis C-string à une position
            // -----------------------------------------------------------------
            /**
             * @brief Insère une C-string à la position spécifiée
             * @param pos Position d'insertion (0 <= pos <= Length())
             * @param str Chaîne C à insérer
             * @return Référence vers this pour chaînage
             * @note Décale les caractères existants vers la droite
             */
            NkBasicString& Insert(SizeType pos, const CharT* str)
            {
                if (!str)
                {
                    return *this;
                }

                SizeType len = Traits::Length(str);

                if (len == 0)
                {
                    return *this;
                }

                NKENTSEU_ASSERT(pos <= mLength);
                GrowIfNeeded(len);

                CharT* data = GetData();
                Traits::Move(data + pos + len, data + pos, mLength - pos);
                Traits::Copy(data + pos, str, len);
                mLength += len;
                data[mLength] = Traits::Null();
                return *this;
            }

            // -----------------------------------------------------------------
            // Insertion depuis NkBasicString à une position
            // -----------------------------------------------------------------
            /**
             * @brief Insère une NkBasicString à la position spécifiée
             * @param pos Position d'insertion
             * @param str Chaîne à insérer
             * @return Référence vers this pour chaînage
             * @note Délègue à Insert(pos, str.Data())
             */
            NkBasicString& Insert(SizeType pos, const NkBasicString& str)
            {
                return Insert(pos, str.Data());
            }

            // -----------------------------------------------------------------
            // Insertion de répétitions d'un caractère
            // -----------------------------------------------------------------
            /**
             * @brief Insère count répétitions de ch à la position spécifiée
             * @param pos Position d'insertion
             * @param count Nombre de répétitions à insérer
             * @param ch Caractère à répéter
             * @return Référence vers this pour chaînage
             * @note Utile pour padding interne ou séparateurs positionnés
             */
            NkBasicString& Insert(SizeType pos, SizeType count, CharT ch)
            {
                NKENTSEU_ASSERT(pos <= mLength);

                if (count == 0)
                {
                    return *this;
                }

                GrowIfNeeded(count);

                CharT* data = GetData();
                Traits::Move(data + pos + count, data + pos, mLength - pos);
                Traits::Fill(data + pos, ch, count);
                mLength += count;
                data[mLength] = Traits::Null();
                return *this;
            }

            // -----------------------------------------------------------------
            // Suppression de caractères (Erase)
            // -----------------------------------------------------------------
            /**
             * @brief Supprime count caractères à partir de la position pos
             * @param pos Position de début de suppression (défaut: 0)
             * @param count Nombre de caractères à supprimer (défaut: npos = jusqu'à la fin)
             * @return Référence vers this pour chaînage
             * @note Décale les caractères suivants vers la gauche pour combler le trou
             */
            NkBasicString& Erase(SizeType pos = 0, SizeType count = npos)
            {
                NKENTSEU_ASSERT(pos <= mLength);

                if (pos == mLength)
                {
                    return *this;
                }

                if (count == npos || pos + count >= mLength)
                {
                    count = mLength - pos;
                }

                CharT* data = GetData();
                Traits::Move(data + pos, data + pos + count, mLength - pos - count + 1);
                mLength -= count;
                return *this;
            }

            // -----------------------------------------------------------------
            // Ajout en fin (PushBack)
            // -----------------------------------------------------------------
            /**
             * @brief Ajoute un caractère à la fin de la chaîne
             * @param ch Caractère à ajouter
             * @note Alias vers Append(ch), nom plus sémantique pour usage type conteneur
             */
            void PushBack(CharT ch)
            {
                Append(ch);
            }

            // -----------------------------------------------------------------
            // Suppression du dernier caractère (PopBack)
            // -----------------------------------------------------------------
            /**
             * @brief Supprime le dernier caractère de la chaîne
             * @note Décrémente Length() et met à jour le nul terminal
             * @warning Assertion si la chaîne est vide
             */
            void PopBack()
            {
                NKENTSEU_ASSERT(mLength > 0);
                --mLength;
                GetData()[mLength] = Traits::Null();
            }

            // -----------------------------------------------------------------
            // Échange de contenu entre deux chaînes
            // -----------------------------------------------------------------
            /**
             * @brief Échange le contenu de cette chaîne avec une autre
             * @param other Chaîne avec laquelle échanger
             * @note Implémentation via move pour efficacité, noexcept
             */
            void Swap(NkBasicString& other) NKENTSEU_NOEXCEPT
            {
                NkBasicString temp(static_cast<NkBasicString&&>(*this));
                *this = static_cast<NkBasicString&&>(other);
                other = static_cast<NkBasicString&&>(temp);
            }

            // =================================================================
            // SECTION 2.6 : OPÉRATIONS DE RECHERCHE ET COMPARAISON
            // =================================================================

            // -----------------------------------------------------------------
            // Extraction de sous-chaîne
            // -----------------------------------------------------------------
            /**
             * @brief Crée une nouvelle chaîne contenant une sous-partie
             * @param pos Position de départ (défaut: 0)
             * @param count Nombre maximal de caractères (défaut: npos = jusqu'à la fin)
             * @return Nouvelle instance NkBasicString contenant la sous-chaîne
             * @note Copie les données, la nouvelle chaîne est indépendante
             */
            NkBasicString SubStr(SizeType pos = 0, SizeType count = npos) const
            {
                NKENTSEU_ASSERT(pos <= mLength);

                if (count == npos || pos + count > mLength)
                {
                    count = mLength - pos;
                }

                return NkBasicString(GetData() + pos, count);
            }

            // -----------------------------------------------------------------
            // Comparaison avec une autre NkBasicString
            // -----------------------------------------------------------------
            /**
             * @brief Compare cette chaîne avec une autre lexicographiquement
             * @param other Chaîne de référence pour la comparaison
             * @return -1 si *this < other, 0 si égales, +1 si *this > other
             * @note Comparaison caractère-par-caractère, sensible à l'encodage
             */
            int Compare(const NkBasicString& other) const NKENTSEU_NOEXCEPT
            {
                SizeType minLen = mLength < other.mLength ? mLength : other.mLength;
                int result = Traits::Compare(GetData(), other.GetData(), minLen);

                if (result != 0)
                {
                    return result;
                }

                if (mLength < other.mLength)
                {
                    return -1;
                }

                if (mLength > other.mLength)
                {
                    return 1;
                }

                return 0;
            }

            // -----------------------------------------------------------------
            // Comparaison avec une C-string
            // -----------------------------------------------------------------
            /**
             * @brief Compare cette chaîne avec une C-string
             * @param str C-string de référence pour la comparaison
             * @return -1/0/+1 selon l'ordre lexicographique
             * @note Crée temporairement un NkBasicString pour la comparaison
             */
            int Compare(const CharT* str) const NKENTSEU_NOEXCEPT
            {
                if (!str)
                {
                    return 1;
                }

                return Compare(NkBasicString(str));
            }

            // -----------------------------------------------------------------
            // Recherche de sous-chaîne depuis C-string
            // -----------------------------------------------------------------
            /**
             * @brief Recherche la première occurrence d'une C-string
             * @param str C-string à rechercher
             * @param pos Position de départ de la recherche (défaut: 0)
             * @return Index de la première occurrence, ou npos si non trouvé
             * @note Algorithme naïf O(n*m), suffisant pour petites recherches
             */
            SizeType Find(const CharT* str, SizeType pos = 0) const NKENTSEU_NOEXCEPT
            {
                if (!str)
                {
                    return npos;
                }

                SizeType len = Traits::Length(str);

                if (len == 0)
                {
                    return pos;
                }

                if (pos > mLength || len > mLength - pos)
                {
                    return npos;
                }

                const CharT* data = GetData();

                for (SizeType i = pos; i <= mLength - len; ++i)
                {
                    if (Traits::Compare(data + i, str, len) == 0)
                    {
                        return i;
                    }
                }

                return npos;
            }

            // -----------------------------------------------------------------
            // Recherche de caractère unique
            // -----------------------------------------------------------------
            /**
             * @brief Recherche la première occurrence d'un caractère
             * @param ch Caractère à rechercher
             * @param pos Position de départ (défaut: 0)
             * @return Index de la première occurrence, ou npos si non trouvé
             * @note Délègue à Traits::Find pour l'implémentation
             */
            SizeType Find(CharT ch, SizeType pos = 0) const NKENTSEU_NOEXCEPT
            {
                const CharT* found = Traits::Find(GetData() + pos, mLength - pos, ch);
                return found ? static_cast<SizeType>(found - GetData()) : npos;
            }

            // -----------------------------------------------------------------
            // Recherche inverse de caractère
            // -----------------------------------------------------------------
            /**
             * @brief Recherche la dernière occurrence d'un caractère
             * @param ch Caractère à rechercher
             * @param pos Position de départ de la recherche inverse (défaut: npos = fin)
             * @return Index de la dernière occurrence <= pos, ou npos si non trouvé
             * @note Recherche effectuée de droite à gauche
             */
            SizeType RFind(CharT ch, SizeType pos = npos) const NKENTSEU_NOEXCEPT
            {
                if (mLength == 0)
                {
                    return npos;
                }

                SizeType searchPos = (pos >= mLength) ? mLength - 1 : pos;
                const CharT* data = GetData();

                for (SizeType i = searchPos + 1; i > 0; --i)
                {
                    if (data[i - 1] == ch)
                    {
                        return i - 1;
                    }
                }

                return npos;
            }

            // -----------------------------------------------------------------
            // Vérification de préfixe
            // -----------------------------------------------------------------
            /**
             * @brief Vérifie si la chaîne commence par le préfixe spécifié
             * @param prefix C-string représentant le préfixe recherché
             * @return true si les premiers caractères correspondent exactement
             * @note Gère nullptr en retournant false
             */
            bool StartsWith(const CharT* prefix) const NKENTSEU_NOEXCEPT
            {
                if (!prefix)
                {
                    return false;
                }

                SizeType len = Traits::Length(prefix);

                if (len > mLength)
                {
                    return false;
                }

                return Traits::Compare(GetData(), prefix, len) == 0;
            }

            // -----------------------------------------------------------------
            // Vérification de suffixe
            // -----------------------------------------------------------------
            /**
             * @brief Vérifie si la chaîne se termine par le suffixe spécifié
             * @param suffix C-string représentant le suffixe recherché
             * @return true si les derniers caractères correspondent exactement
             * @note Gère nullptr en retournant false
             */
            bool EndsWith(const CharT* suffix) const NKENTSEU_NOEXCEPT
            {
                if (!suffix)
                {
                    return false;
                }

                SizeType len = Traits::Length(suffix);

                if (len > mLength)
                {
                    return false;
                }

                return Traits::Compare(GetData() + mLength - len, suffix, len) == 0;
            }

            // -----------------------------------------------------------------
            // Vérification de présence d'un caractère
            // -----------------------------------------------------------------
            /**
             * @brief Vérifie si la chaîne contient le caractère spécifié
             * @param ch Caractère à rechercher
             * @return true si Find(ch) != npos, false sinon
             * @note Alias sémantique pour meilleure lisibilité
             */
            bool Contains(CharT ch) const NKENTSEU_NOEXCEPT
            {
                return Find(ch) != npos;
            }

            // =================================================================
            // SECTION 2.7 : MÉTHODES SPÉCIFIQUES À L'ENCODAGE
            // =================================================================

            // -----------------------------------------------------------------
            // Nombre d'unités de code (code units)
            // -----------------------------------------------------------------
            /**
             * @brief Retourne le nombre d'unités de code dans la chaîne
             * @return Équivalent à Length() pour cette implémentation
             * @note Pour UTF-8/16/32 : une unité de code = un CharT
             *       Ne pas confondre avec le nombre de codepoints Unicode
             */
            SizeType CodeUnitCount() const NKENTSEU_NOEXCEPT
            {
                return Length();
            }

            // -----------------------------------------------------------------
            // Nombre de points de code (code points) — approximation
            // -----------------------------------------------------------------
            /**
             * @brief Retourne une estimation du nombre de codepoints Unicode
             * @return Actuellement équivalent à Length() (approximation)
             * @warning Pour UTF-8/16, un codepoint peut nécessiter plusieurs CharT
             *          Cette méthode devrait être implémentée avec un décodeur Unicode
             */
            SizeType CodePointCount() const NKENTSEU_NOEXCEPT
            {
                return mLength;
            }

        private:
            // -----------------------------------------------------------------
            // MEMBRES PRIVÉS : données internes
            // -----------------------------------------------------------------

            /**
             * @brief Pointeur vers l'allocateur utilisé pour cette instance
             * @note Peut pointer vers l'allocateur par défaut ou un custom allocator
             */
            memory::NkIAllocator* mAllocator;

            /**
             * @brief Nombre actuel de caractères dans la chaîne
             */
            SizeType mLength;

            /**
             * @brief Capacité actuelle (SSO_CAPACITY ou capacité heap allouée)
             * @note Utilisé aussi comme flag : si == SSO_CAPACITY → mode SSO actif
             */
            SizeType mCapacity;

            // -----------------------------------------------------------------
            // Union pour stockage SSO ou heap (mémoire partagée)
            // -----------------------------------------------------------------
            union
            {
                /**
                 * @brief Pointeur vers les données heap (si !IsSSO())
                 */
                CharT* mHeapData;

                /**
                 * @brief Buffer interne pour Small String Optimization
                 * @note Taille : SSO_CAPACITY + 1 pour le nul terminal
                 */
                CharT mSSOData[SSO_CAPACITY + 1];
            };

            // -----------------------------------------------------------------
            // Helpers privés pour gestion SSO/heap
            // -----------------------------------------------------------------

            /**
             * @brief Vérifie si la chaîne utilise le stockage SSO
             * @return true si mCapacity == SSO_CAPACITY, false sinon
             * @note noexcept, utilisé intensivement dans les accès
             */
            bool IsSSO() const NKENTSEU_NOEXCEPT
            {
                return mCapacity == SSO_CAPACITY;
            }

            /**
             * @brief Définit le mode SSO en ajustant mCapacity
             * @param sso true pour activer le mode SSO, false pour heap
             * @note noexcept, opération interne de basculement
             */
            void SetSSO(bool sso) NKENTSEU_NOEXCEPT
            {
                if (sso)
                {
                    mCapacity = SSO_CAPACITY;
                }
            }

            /**
             * @brief Retourne un pointeur vers les données actives (SSO ou heap)
             * @return Pointeur modifiable vers le buffer de données
             * @note noexcept, méthode critique appelée fréquemment
             */
            CharT* GetData() NKENTSEU_NOEXCEPT
            {
                return IsSSO() ? mSSOData : mHeapData;
            }

            /**
             * @brief Retourne un pointeur constant vers les données actives
             * @return Pointeur constant vers le buffer de données
             * @note Version const pour les méthodes en lecture seule
             */
            const CharT* GetData() const NKENTSEU_NOEXCEPT
            {
                return IsSSO() ? mSSOData : mHeapData;
            }

            // -----------------------------------------------------------------
            // Gestion mémoire heap privée
            // -----------------------------------------------------------------

            /**
             * @brief Alloue un buffer heap de capacité spécifiée
             * @param capacity Nombre de caractères à pouvoir stocker (hors nul terminal)
             * @note Met à jour mHeapData, mCapacity et le flag SSO
             */
            void AllocateHeap(SizeType capacity)
            {
                mHeapData = static_cast<CharT*>(
                    mAllocator->Allocate((capacity + 1) * sizeof(CharT))
                );
                mCapacity = capacity;
                SetSSO(false);
            }

            /**
             * @brief Libère le buffer heap si actif
             * @note Vérifie IsSSO() avant désallocation pour sécurité
             */
            void DeallocateHeap()
            {
                if (!IsSSO() && mHeapData)
                {
                    mAllocator->Deallocate(mHeapData);
                    mHeapData = nullptr;
                }
            }

            // -----------------------------------------------------------------
            // Croissance automatique du buffer
            // -----------------------------------------------------------------
            /**
             * @brief Augmente la capacité si nécessaire pour accueillir additionalSize caractères
             * @param additionalSize Nombre de caractères supplémentaires nécessaires
             * @note Stratégie de croissance : nouvelle capacité = ancienne + 50%, minimum needed
             */
            void GrowIfNeeded(SizeType additionalSize)
            {
                SizeType needed = mLength + additionalSize;

                if (needed <= mCapacity)
                {
                    return;
                }

                SizeType newCapacity = mCapacity + (mCapacity / 2);

                if (newCapacity < needed)
                {
                    newCapacity = needed;
                }

                Reserve(newCapacity);
            }
        };

        // =====================================================================
        // SECTION 3 : ALIASES DE TYPES PRÉ-DÉFINIS PAR ENCODAGE
        // =====================================================================

        // -----------------------------------------------------------------
        // Alias pour chaîne UTF-8 (char)
        // -----------------------------------------------------------------
        /**
         * @brief Alias pour NkBasicString<char> — encodage UTF-8 recommandé
         * @note Type par défaut pour la majorité des usages dans le projet
         */
        using NkString8 = NkBasicString<Char>;

        // -----------------------------------------------------------------
        // Alias pour chaîne UTF-16 (char16_t)
        // -----------------------------------------------------------------
        /**
         * @brief Alias pour NkBasicString<char16_t> — encodage UTF-16
         * @note Utile pour l'interopérabilité avec les API Windows ou Java
         */
        using NkString16 = NkBasicString<char16>;

        // -----------------------------------------------------------------
        // Alias pour chaîne UTF-32 (char32_t)
        // -----------------------------------------------------------------
        /**
         * @brief Alias pour NkBasicString<char32_t> — encodage UTF-32
         * @note Chaque codepoint Unicode tient sur un seul CharT, mais mémoire intensive
         */
        using NkString32 = NkBasicString<char32>;

        // -----------------------------------------------------------------
        // Alias pour chaîne wide (wchar_t, plateforme-dépendant)
        // -----------------------------------------------------------------
        /**
         * @brief Alias pour NkBasicString<wchar_t> — encodage wide natif
         * @note wchar_t = 2 octets sur Windows (UTF-16), 4 octets sur Linux (UTF-32)
         * @warning À éviter pour du code portable, préférer NkString8/16/32 explicites
         */
        using NkWString = NkBasicString<wchar>;

        // -----------------------------------------------------------------
        // Alias par défaut du projet (décommenter pour changer le standard)
        // -----------------------------------------------------------------
        // using NkString = NkString8;  // UTF-8 par défaut

        // =====================================================================
        // SECTION 4 : DÉCLARATIONS DE FONCTIONS DE CONVERSION D'ENCODAGE
        // =====================================================================

        // -----------------------------------------------------------------
        // Conversions UTF-8 → autres encodages
        // -----------------------------------------------------------------
        /**
         * @brief Convertit une chaîne UTF-8 vers UTF-16
         * @param utf8Str Chaîne source en UTF-8
         * @return Nouvelle chaîne en UTF-16
         * @note Gère les séquences multi-octets UTF-8 correctement
         */
        NKENTSEU_CONTAINERS_API NkString16 NkToUTF16(const NkString8& utf8Str);

        /**
         * @brief Convertit une chaîne UTF-8 vers UTF-32
         * @param utf8Str Chaîne source en UTF-8
         * @return Nouvelle chaîne en UTF-32
         * @note Chaque codepoint Unicode devient un char32_t
         */
        NKENTSEU_CONTAINERS_API NkString32 NkToUTF32(const NkString8& utf8Str);

        // -----------------------------------------------------------------
        // Conversions autres encodages → UTF-8
        // -----------------------------------------------------------------
        /**
         * @brief Convertit une chaîne UTF-16 vers UTF-8
         * @param utf16Str Chaîne source en UTF-16
         * @return Nouvelle chaîne en UTF-8
         * @note Gère les surrogate pairs UTF-16 correctement
         */
        NKENTSEU_CONTAINERS_API NkString8 NkToUTF8(const NkString16& utf16Str);

        /**
         * @brief Convertit une chaîne UTF-32 vers UTF-8
         * @param utf32Str Chaîne source en UTF-32
         * @return Nouvelle chaîne en UTF-8
         */
        NKENTSEU_CONTAINERS_API NkString8 NkToUTF8(const NkString32& utf32Str);

        // -----------------------------------------------------------------
        // Conversions avec wchar_t (plateforme-dépendant)
        // -----------------------------------------------------------------
        /**
         * @brief Convertit une chaîne UTF-8 vers wide string natif
         * @param str Chaîne source en UTF-8
         * @return Nouvelle chaîne en wchar_t
         * @warning Résultat dépend de la plateforme (UTF-16 sur Windows, UTF-32 sur Linux)
         */
        NKENTSEU_CONTAINERS_API NkWString NkToWide(const NkString8& str);

        /**
         * @brief Convertit une wide string natif vers UTF-8
         * @param wstr Chaîne source en wchar_t
         * @return Nouvelle chaîne en UTF-8
         * @warning Interprète wchar_t selon l'encodage natif de la plateforme
         */
        NKENTSEU_CONTAINERS_API NkString8 NkFromWide(const NkWString& wstr);

    } // namespace nkentseu

#endif // NK_CONTAINERS_STRING_NKBASICSTRING_H

// =============================================================================
// EXEMPLES D'UTILISATION
// =============================================================================
/*
    // -------------------------------------------------------------------------
    // Exemple 1 : Création et manipulation basique
    // -------------------------------------------------------------------------
    #include "NKContainers/String/NkBasicString.h"
    using namespace nkentseu;

    // Construction depuis littéral
    NkString8 str1 = "Hello";
    
    // Construction avec répétition
    NkString8 separators(10, '-');  // "----------"
    
    // Concaténation chaînée
    NkString8 greeting = NkString8("Hello") + ", " + "World" + '!';
    // Résultat : "Hello, World!"

    // Accès aux éléments
    char first = greeting.Front();        // 'H'
    char last = greeting.Back();          // '!'
    char third = greeting[2];             // 'l'

    // -------------------------------------------------------------------------
    // Exemple 2 : Modification in-place
    // -------------------------------------------------------------------------
    NkString8 path = "/usr/local/bin";
    
    // Insertion au début
    path.Insert(0, "sudo ");  // "/usr/local/bin" → "sudo /usr/local/bin"
    
    // Remplacement via Erase + Insert
    path.Erase(0, 5);  // Supprime "sudo "
    path.Insert(0, "env ");  // Insère "env "
    
    // Suppression du suffixe
    if (path.EndsWith("/bin"))
    {
        path.Erase(path.Length() - 4);  // Supprime "/bin"
    }
    
    // Ajout en fin
    path.PushBack('/');
    path.Append("myapp");  // path = "env /usr/local/myapp"

    // -------------------------------------------------------------------------
    // Exemple 3 : Recherche et comparaison
    // -------------------------------------------------------------------------
    NkString8 text = "The quick brown fox jumps over the lazy dog";
    
    // Recherche de mot
    usize pos = text.Find("fox");  // retourne 16
    
    // Vérification de présence
    if (text.Contains("quick") && text.Contains("lazy"))
    {
        // Les deux mots-clés sont présents
    }
    
    // Comparaison lexicographique
    NkString8 a = "apple";
    NkString8 b = "banana";
    
    if (a.Compare(b) < 0)
    {
        // "apple" < "banana" dans l'ordre alphabétique
    }
    
    // Tri de conteneur
    std::vector<NkString8> words = { "zebra", "apple", "mango" };
    std::sort(words.begin(), words.end(), 
        [](const NkString8& x, const NkString8& y) { return x.Compare(y) < 0; });

    // -------------------------------------------------------------------------
    // Exemple 4 : Gestion mémoire et performances
    // -------------------------------------------------------------------------
    // Pré-allocation pour éviter les réallocations multiples
    NkString8 buffer;
    buffer.Reserve(1024);  // Réserve 1024 caractères d'un coup
    
    for (int i = 0; i < 100; ++i)
    {
        buffer.Append("Line ");
        buffer.Append(i);  // Suppose Append(int) existe ou convertir d'abord
        buffer.Append('\n');
        // Aucune allocation supplémentaire grâce à Reserve()
    }
    
    // Réduction de capacité après usage pour libérer la mémoire
    buffer.ShrinkToFit();  // Libère l'excédent de capacité

    // -------------------------------------------------------------------------
    // Exemple 5 : Support multi-encodage
    // -------------------------------------------------------------------------
    // UTF-16 pour interop Windows
    NkString16 wstr = u"Bonjour le monde";
    if (wstr.StartsWith(u"Bon"))
    {
        // Préfixe détecté en UTF-16
    }
    
    // Conversion d'encodage
    NkString8 utf8 = NkToUTF8(wstr);  // UTF-16 → UTF-8
    NkString16 back = NkToUTF16(utf8);  // UTF-8 → UTF-16
    
    // UTF-32 pour traitement de codepoints Unicode
    NkString32 emojis = U"🎉🎊🎈";
    // Note : Length() retourne 3 (codepoints), pas le nombre d'octets

    // -------------------------------------------------------------------------
    // Exemple 6 : Utilisation avec allocateur personnalisé
    // -------------------------------------------------------------------------
    // Dans un contexte de jeu ou temps réel avec pool allocator
    memory::NkPoolAllocator frameAlloc(4096);  // Pool de 4KB
    
    NkString8 debugMsg(frameAlloc);
    debugMsg.Append("[Frame ");
    debugMsg.Append(currentFrame);  // Suppose conversion int → string
    debugMsg.Append("] FPS: ");
    debugMsg.Append(currentFPS, 2);  // Suppose Append avec précision
    
    // La mémoire est allouée depuis frameAlloc, pas l'allocateur système
    RenderDebug(debugMsg.CStr());
    
    // frameAlloc est vidé en fin de frame, libérant toutes les chaînes d'un coup

    // -------------------------------------------------------------------------
    // Exemple 7 : Bonnes pratiques et pièges à éviter
    // -------------------------------------------------------------------------

    // ✅ BON : Utiliser Reserve() avant une boucle de concaténation
    NkString8 csv;
    csv.Reserve(256);  // Estimation de taille
    for (const auto& item : items)
    {
        csv.Append(item);
        csv.Append(',');
    }
    csv.PopBack();  // Supprime la dernière virgule

    // ❌ MAUVAIS : Concaténation naïve dans une boucle (réallocations multiples)
    // NkString8 csv;
    // for (const auto& item : items)
    // {
    //     csv = csv + item + ",";  // Allocation + copie à chaque itération !
    // }

    // ✅ BON : Utiliser SubStr() pour extraire sans modifier l'original
    NkString8 url = "https://example.com/path/to/resource";
    NkString8 domain = url.SubStr(8, url.Find('/', 8) - 8);  // "example.com"
    // url reste inchangé

    // ✅ BON : Vérifier Empty() avant Front()/Back()
    void ProcessFirstChar(const NkString8& str)
    {
        if (!str.Empty())
        {
            char c = str.Front();  // Sécurisé
            HandleChar(c);
        }
    }

    // ⚠️ ATTENTION : NkBasicString n'est pas thread-safe
    // Partager une instance entre threads nécessite synchronisation externe
    // Privilégier la copie ou le passage par valeur pour la sécurité

    // -------------------------------------------------------------------------
    // Exemple 8 : Interopérabilité avec API C
    // -------------------------------------------------------------------------
    // Passage à printf avec précision
    NkString8 msg = "Error code: ";
    msg.Append(errorCode);  // Suppose conversion int
    printf("%.*s\n", static_cast<int>(msg.Length()), msg.CStr());
    
    // Passage à une API C attendants char*
    void LegacyAPI(const char* input);
    LegacyMsg(msg.CStr());  // CStr() retourne char* nul-terminé
    
    // Attention : ne pas modifier le buffer retourné par Data()/CStr()
    // sans mettre à jour Length() manuellement (non recommandé)
*/

// ============================================================
// Copyright © 2024-2026 Rihen. Tous droits réservés.
// Licence Propriétaire - Utilisation et modification autorisées
//
// Généré par Rihen le 2026-02-07
// Date de création : 2026-02-07
// Dernière modification : 2026-04-26
// ============================================================