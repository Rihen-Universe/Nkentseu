// -----------------------------------------------------------------------------
// FICHIER: Core\NKContainers\src\NKContainers\String\NkStringBuilder.cpp
// DESCRIPTION: Implémentation de la classe NkStringBuilder pour concaténations efficaces
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

// -------------------------------------------------------------------------
// Inclusion de l'en-tête correspondant
// -------------------------------------------------------------------------
#include "NkStringBuilder.h"

// -------------------------------------------------------------------------
// Inclusions des utilitaires de chaînes
// -------------------------------------------------------------------------
#include "NkStringUtils.h"
#include "NkStringHash.h"
#include "NKMemory/NkFunction.h"
#include "NKCore/Assert/NkAssert.h"

// -------------------------------------------------------------------------
// Inclusions standard
// -------------------------------------------------------------------------
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>

// -------------------------------------------------------------------------
// Namespace principal du projet
// -------------------------------------------------------------------------
namespace nkentseu {

    // =====================================================================
    // SECTION : FONCTIONS HELPER STATIQUES (CONVERSIONS NUMÉRIQUES)
    // =====================================================================

    /**
     * @brief Convertit un entier signé 64 bits en chaîne décimale
     * 
     * @param value Valeur entière à convertir
     * @param buffer Pointeur vers le buffer de destination
     * @param bufferSize Taille disponible du buffer destination
     * @return Nombre de caractères écrits (exclut null-terminator), ou 0 si échec
     * 
     * @note Utilise snprintf avec format "%lld" pour compatibilité multi-plateforme.
     *       Retourne 0 si bufferSize insuffisante ou erreur de conversion.
     */
    NkStringBuilder::SizeType NkStringBuilder::IntToString(
        int64 value, 
        char* buffer, 
        SizeType bufferSize) {
        
        int len = std::snprintf(buffer, bufferSize, "%lld", static_cast<long long>(value));
        return len >= 0 ? static_cast<SizeType>(len) : 0;
    }

    /**
     * @brief Convertit un entier non-signé 64 bits en chaîne décimale
     * 
     * @param value Valeur entière à convertir
     * @param buffer Pointeur vers le buffer de destination
     * @param bufferSize Taille disponible du buffer destination
     * @return Nombre de caractères écrits (exclut null-terminator), ou 0 si échec
     * 
     * @note Utilise snprintf avec format "%llu" pour compatibilité multi-plateforme.
     */
    NkStringBuilder::SizeType NkStringBuilder::UIntToString(
        uint64 value, 
        char* buffer, 
        SizeType bufferSize) {
        
        int len = std::snprintf(
            buffer, 
            bufferSize, 
            "%llu", 
            static_cast<unsigned long long>(value));
        return len >= 0 ? static_cast<SizeType>(len) : 0;
    }

    /**
     * @brief Convertit un flottant 64 bits en chaîne décimale avec précision
     * 
     * @param value Valeur flottante à convertir
     * @param buffer Pointeur vers le buffer de destination
     * @param bufferSize Taille disponible du buffer destination
     * @param precision Nombre de décimales à afficher (défaut: 6)
     * @return Nombre de caractères écrits (exclut null-terminator), ou 0 si échec
     * 
     * @note Construit dynamiquement la chaîne de format "%.Nlf" puis utilise snprintf.
     *       Accepte notation scientifique pour valeurs extrêmes.
     */
    NkStringBuilder::SizeType NkStringBuilder::FloatToString(
        float64 value, 
        char* buffer, 
        SizeType bufferSize, 
        int precision) {
        
        char format[16];
        std::snprintf(format, sizeof(format), "%%.%dlf", precision);
        int len = std::snprintf(buffer, bufferSize, format, value);
        return len >= 0 ? static_cast<SizeType>(len) : 0;
    }

    /**
     * @brief Convertit un entier non-signé 64 bits en hexadécimal
     * 
     * @param value Valeur entière à convertir
     * @param buffer Pointeur vers le buffer de destination
     * @param bufferSize Taille disponible du buffer destination
     * @param prefix true pour ajouter le préfixe "0x", false sinon
     * @return Nombre de caractères écrits (exclut null-terminator), ou 0 si échec
     * 
     * @note Chiffres en majuscules : "FF", "A1B2", etc.
     *       Format compatible avec les conventions C/C++.
     */
    NkStringBuilder::SizeType NkStringBuilder::HexToString(
        uint64 value, 
        char* buffer, 
        SizeType bufferSize, 
        bool prefix) {
        
        if (prefix) {
            int len = std::snprintf(
                buffer, 
                bufferSize, 
                "0x%llX", 
                static_cast<unsigned long long>(value));
            return len >= 0 ? static_cast<SizeType>(len) : 0;
        } else {
            int len = std::snprintf(
                buffer, 
                bufferSize, 
                "%llX", 
                static_cast<unsigned long long>(value));
            return len >= 0 ? static_cast<SizeType>(len) : 0;
        }
    }

    /**
     * @brief Convertit un entier non-signé 64 bits en binaire avec espaces optionnels
     * 
     * @param value Valeur entière à convertir
     * @param buffer Pointeur vers le buffer de destination
     * @param bufferSize Taille disponible du buffer destination
     * @param bits Nombre de bits à afficher (1-64)
     * @return Nombre de caractères écrits (exclut null-terminator)
     * 
     * @note Ajoute automatiquement des espaces tous les 4 bits pour lisibilité.
     *       Ex: BinaryToString(0xF0F0, buffer, 32, 16) -> "1111 0000 1111 0000"
     *       Padding avec zéros à gauche si bits > largeur réelle.
     */
    NkStringBuilder::SizeType NkStringBuilder::BinaryToString(
        uint64 value, 
        char* buffer, 
        SizeType bufferSize, 
        SizeType bits) {
        
        if (bits > 64) {
            bits = 64;
        }
        if (bits == 0) {
            return 0;
        }
        char* ptr = buffer;
        for (SizeType i = bits; i > 0; --i) {
            *ptr++ = (value & (1ULL << (i - 1))) ? '1' : '0';
            if ((i - 1) % 4 == 0 && i > 1) {
                *ptr++ = ' ';
            }
        }
        *ptr = '\0';
        return static_cast<SizeType>(ptr - buffer);
    }

    /**
     * @brief Convertit un entier non-signé 64 bits en octal
     * 
     * @param value Valeur entière à convertir
     * @param buffer Pointeur vers le buffer de destination
     * @param bufferSize Taille disponible du buffer destination
     * @param prefix true pour ajouter le préfixe "0", false sinon
     * @return Nombre de caractères écrits (exclut null-terminator), ou 0 si échec
     * 
     * @note Format compatible avec les conventions C/C++.
     *       Ex: OctalToString(64, buffer, 32, true) -> "0100"
     */
    NkStringBuilder::SizeType NkStringBuilder::OctalToString(
        uint64 value, 
        char* buffer, 
        SizeType bufferSize, 
        bool prefix) {
        
        if (prefix) {
            int len = std::snprintf(
                buffer, 
                bufferSize, 
                "0%llo", 
                static_cast<unsigned long long>(value));
            return len >= 0 ? static_cast<SizeType>(len) : 0;
        } else {
            int len = std::snprintf(
                buffer, 
                bufferSize, 
                "%llo", 
                static_cast<unsigned long long>(value));
            return len >= 0 ? static_cast<SizeType>(len) : 0;
        }
    }

    // =====================================================================
    // SECTION : FONCTIONS HELPER PRIVÉES (GESTION MÉMOIRE)
    // =====================================================================

    /**
     * @brief Libère le buffer interne si alloué sur le heap
     * 
     * @note No-op si mBuffer est nullptr.
     *       Met mBuffer à nullptr après libération pour éviter double-free.
     *       Utilise l'allocateur associé à cette instance.
     */
    void NkStringBuilder::FreeBuffer() {
        if (mBuffer) {
            mAllocator->Deallocate(mBuffer);
            mBuffer = nullptr;
        }
    }

    /**
     * @brief Alloue un nouveau buffer de capacité donnée
     * 
     * @param capacity Nouvelle capacité souhaitée (en caractères)
     * 
     * @note Utilise mAllocator pour l'allocation.
     *       Initialise le buffer avec null-terminator en position 0.
     *       Réinitialise mLength à 0 car le buffer est vide après allocation.
     */
    void NkStringBuilder::AllocateBuffer(SizeType capacity) {
        mBuffer = static_cast<char*>(
            mAllocator->Allocate((capacity + 1) * sizeof(char)));
        mCapacity = capacity;
        mBuffer[0] = '\0';
        mLength = 0;
    }

    /**
     * @brief Copie le contenu d'un autre builder dans this
     * 
     * @param other Référence constante vers le builder source
     * 
     * @note Alloue un nouveau buffer via AllocateBuffer() si nécessaire.
     *       Copie les données existantes avec NkMemCopy().
     *       Préserve l'allocateur de other (pas celui courant de this).
     */
    void NkStringBuilder::CopyFrom(const NkStringBuilder& other) {
        mAllocator = other.mAllocator;
        mLength = other.mLength;
        mCapacity = other.mCapacity;
        if (mCapacity > 0) {
            mBuffer = static_cast<char*>(
                mAllocator->Allocate((mCapacity + 1) * sizeof(char)));
            if (other.mBuffer && mLength > 0) {
                memory::NkMemCopy(mBuffer, other.mBuffer, mLength);
            }
            mBuffer[mLength] = '\0';
        } else {
            mBuffer = nullptr;
        }
    }

    /**
     * @brief Déplace le contenu d'un autre builder dans this
     * 
     * @param other Référence rvalue vers le builder source à déplacer
     * 
     * @note Transfère la propriété du buffer de other vers this.
     *       Réinitialise other à un état vide (buffer=nullptr, length=0, capacity=0).
     *       Complexité : O(1), aucune copie de données.
     */
    void NkStringBuilder::MoveFrom(NkStringBuilder&& other) noexcept {
        mAllocator = other.mAllocator;
        mBuffer = other.mBuffer;
        mLength = other.mLength;
        mCapacity = other.mCapacity;
        other.mBuffer = nullptr;
        other.mLength = 0;
        other.mCapacity = 0;
    }

    /**
     * @brief Garantit que la capacité peut accueillir la taille demandée
     * 
     * @param needed Taille minimale requise (en caractères)
     * 
     * @note Si needed <= mCapacity : no-op.
     *       Sinon : appelle Reallocate() avec la nouvelle capacité.
     */
    void NkStringBuilder::EnsureCapacity(SizeType needed) {
        if (needed <= mCapacity) {
            return;
        }
        Reallocate(needed);
    }

    /**
     * @brief Garantit que la capacité peut accueillir des données supplémentaires
     * 
     * @param additionalSize Nombre de caractères supplémentaires prévus
     * 
     * @note Calcule la capacité nécessaire : mLength + additionalSize.
     *       Applique une stratégie de croissance géométrique (1.5x) pour amortir
     *       le coût des réallocations multiples.
     *       Appelle Reallocate() si la capacité actuelle est insuffisante.
     */
    void NkStringBuilder::GrowIfNeeded(SizeType additionalSize) {
        SizeType needed = mLength + additionalSize;
        if (needed <= mCapacity) {
            return;
        }
        // Stratégie de croissance : facteur 1.5x
        SizeType newCapacity = mCapacity + (mCapacity / 2);
        if (newCapacity < needed) {
            newCapacity = needed;
        }
        Reallocate(newCapacity);
    }

    /**
     * @brief Réalloue le buffer à une nouvelle capacité
     * 
     * @param newCapacity Nouvelle capacité souhaitée (en caractères)
     * 
     * @note Alloue un nouveau buffer via mAllocator.
     *       Copie les données existantes avec NkMemCopy().
     *       Libère l'ancien buffer avec Deallocate().
     *       Met à jour mBuffer et mCapacity.
     *       Préserve mLength et garantit null-termination.
     */
    void NkStringBuilder::Reallocate(SizeType newCapacity) {
        if (newCapacity <= mCapacity) {
            return;
        }
        char* newBuffer = static_cast<char*>(
            mAllocator->Allocate((newCapacity + 1) * sizeof(char)));
        if (mBuffer) {
            if (mLength > 0) {
                memory::NkMemCopy(newBuffer, mBuffer, mLength);
            }
            mAllocator->Deallocate(mBuffer);
        }
        mBuffer = newBuffer;
        mCapacity = newCapacity;
        mBuffer[mLength] = '\0';
    }

    // =====================================================================
    // SECTION : CONSTRUCTEURS ET DESTRUCTEUR
    // =====================================================================

    /**
     * @brief Constructeur par défaut
     * 
     * @note Initialise un builder vide avec capacité DEFAULT_CAPACITY (256).
     *       Utilise l'allocateur par défaut du système.
     *       Appelle Reserve(DEFAULT_CAPACITY) pour allouer le buffer initial.
     */
    NkStringBuilder::NkStringBuilder()
        : mAllocator(&memory::NkGetDefaultAllocator())
        , mBuffer(nullptr)
        , mLength(0)
        , mCapacity(0) {
        Reserve(DEFAULT_CAPACITY);
    }

    /**
     * @brief Constructeur avec capacité initiale personnalisée
     * 
     * @param initialCapacity Taille initiale du buffer interne
     * 
     * @note Utilise l'allocateur par défaut.
     *       Appelle Reserve(initialCapacity) pour allouer le buffer initial.
     */
    NkStringBuilder::NkStringBuilder(SizeType initialCapacity)
        : mAllocator(&memory::NkGetDefaultAllocator())
        , mBuffer(nullptr)
        , mLength(0)
        , mCapacity(0) {
        Reserve(initialCapacity);
    }

    /**
     * @brief Constructeur avec allocateur personnalisé
     * 
     * @param allocator Référence vers l'allocateur à utiliser
     * 
     * @note Toutes les allocations futures utiliseront cet allocateur.
     *       Appelle Reserve(DEFAULT_CAPACITY) pour allouer le buffer initial.
     */
    NkStringBuilder::NkStringBuilder(memory::NkIAllocator& allocator)
        : mAllocator(&allocator)
        , mBuffer(nullptr)
        , mLength(0)
        , mCapacity(0) {
        Reserve(DEFAULT_CAPACITY);
    }

    /**
     * @brief Constructeur complet : capacité + allocateur
     * 
     * @param initialCapacity Taille initiale du buffer interne
     * @param allocator Référence vers l'allocateur à utiliser
     * 
     * @note Combine les deux options précédentes pour contrôle maximal.
     *       Appelle Reserve(initialCapacity) pour allouer le buffer initial.
     */
    NkStringBuilder::NkStringBuilder(
        SizeType initialCapacity, 
        memory::NkIAllocator& allocator)
        : mAllocator(&allocator)
        , mBuffer(nullptr)
        , mLength(0)
        , mCapacity(0) {
        Reserve(initialCapacity);
    }

    /**
     * @brief Constructeur depuis NkStringView
     * 
     * @param view Vue de chaîne à copier dans le builder
     * 
     * @note Initialise le builder avec le contenu de view.
     *       La vue source peut être détruite après construction.
     *       Utilise l'allocateur par défaut.
     *       Délègue à Append(view) pour la copie des données.
     */
    NkStringBuilder::NkStringBuilder(NkStringView view)
        : mAllocator(&memory::NkGetDefaultAllocator())
        , mBuffer(nullptr)
        , mLength(0)
        , mCapacity(0) {
        Append(view);
    }

    /**
     * @brief Constructeur depuis chaîne C terminée par null
     * 
     * @param str Pointeur vers une chaîne C-style (const char*)
     * 
     * @note Copie les données de str dans le builder.
     *       Si str est nullptr : initialise un builder vide avec capacité par défaut.
     *       Utilise l'allocateur par défaut.
     */
    NkStringBuilder::NkStringBuilder(const char* str)
        : mAllocator(&memory::NkGetDefaultAllocator())
        , mBuffer(nullptr)
        , mLength(0)
        , mCapacity(0) {
        if (str) {
            Append(str);
        } else {
            Reserve(DEFAULT_CAPACITY);
        }
    }

    /**
     * @brief Constructeur depuis NkString
     * 
     * @param str Instance NkString à copier dans le builder
     * 
     * @note Copie les données de str dans le builder.
     *       Plus efficace que conversion str -> view -> builder.
     *       Utilise l'allocateur par défaut.
     */
    NkStringBuilder::NkStringBuilder(const NkString& str)
        : mAllocator(&memory::NkGetDefaultAllocator())
        , mBuffer(nullptr)
        , mLength(0)
        , mCapacity(0) {
        Append(str);
    }

    /**
     * @brief Constructeur de copie
     * 
     * @param other Instance NkStringBuilder à copier
     * 
     * @note Effectue une copie profonde du buffer interne.
     *       Préserve la capacité et l'allocateur de other.
     *       Utilise l'allocateur par défaut comme fallback (devrait être corrigé).
     */
    NkStringBuilder::NkStringBuilder(const NkStringBuilder& other)
        : mAllocator(&memory::NkGetDefaultAllocator())
        , mBuffer(nullptr)
        , mLength(0)
        , mCapacity(0) {
        CopyFrom(other);
    }

    /**
     * @brief Constructeur de déplacement
     * 
     * @param other Instance NkStringBuilder à déplacer
     * 
     * @note Transfère la propriété du buffer de other vers this.
     *       other devient vide et réinitialisé après l'opération.
     *       Complexité : O(1), aucune copie de données.
     *       Utilise traits::NkMove pour transfert sécurisé.
     */
    NkStringBuilder::NkStringBuilder(NkStringBuilder&& other) noexcept
        : mAllocator(&memory::NkGetDefaultAllocator())
        , mBuffer(nullptr)
        , mLength(0)
        , mCapacity(0) {
        MoveFrom(traits::NkMove(other));
    }

    /**
     * @brief Destructeur
     * 
     * @note Libère le buffer interne si alloué sur le heap.
     *       Garantit aucune fuite mémoire.
     */
    NkStringBuilder::~NkStringBuilder() {
        FreeBuffer();
    }

    // =====================================================================
    // SECTION : OPÉRATEURS D'AFFECTATION
    // =====================================================================

    /**
     * @brief Affectation par copie
     * 
     * @param other Instance source à copier
     * @return Référence vers this pour chaînage
     * 
     * @note Gère l'auto-affectation via vérification d'adresse.
     *       Libère le buffer courant avec FreeBuffer().
     *       Copie le contenu de other avec CopyFrom().
     */
    NkStringBuilder& NkStringBuilder::operator=(const NkStringBuilder& other) {
        if (this != &other) {
            FreeBuffer();
            CopyFrom(other);
        }
        return *this;
    }

    /**
     * @brief Affectation par déplacement
     * 
     * @param other Instance source à déplacer
     * @return Référence vers this pour chaînage
     * 
     * @note Gère l'auto-affectation via vérification d'adresse.
     *       Libère le buffer courant avec FreeBuffer().
     *       Transfère les ressources de other avec MoveFrom().
     */
    NkStringBuilder& NkStringBuilder::operator=(NkStringBuilder&& other) noexcept {
        if (this != &other) {
            FreeBuffer();
            MoveFrom(traits::NkMove(other));
        }
        return *this;
    }

    /**
     * @brief Affectation depuis NkStringView
     * 
     * @param view Vue de chaîne à copier
     * @return Référence vers this pour chaînage
     * 
     * @note Remplace le contenu actuel par une copie de view.
     *       Équivalent à Clear() + Append(view).
     */
    NkStringBuilder& NkStringBuilder::operator=(NkStringView view) {
        Clear();
        Append(view);
        return *this;
    }

    /**
     * @brief Affectation depuis chaîne C-style
     * 
     * @param str Chaîne C terminée par null à copier
     * @return Référence vers this pour chaînage
     * 
     * @note Remplace le contenu actuel par une copie de str.
     *       Gère le cas str == nullptr (devient builder vide).
     */
    NkStringBuilder& NkStringBuilder::operator=(const char* str) {
        Clear();
        if (str) {
            Append(str);
        }
        return *this;
    }

    /**
     * @brief Affectation depuis NkString
     * 
     * @param str Instance NkString à copier
     * @return Référence vers this pour chaînage
     * 
     * @note Remplace le contenu actuel par une copie de str.
     *       Équivalent à Clear() + Append(str).
     */
    NkStringBuilder& NkStringBuilder::operator=(const NkString& str) {
        Clear();
        Append(str);
        return *this;
    }

    // =====================================================================
    // SECTION : OPÉRATIONS D'AJOUT - CHAÎNES ET CARACTÈRES
    // =====================================================================

    /**
     * @brief Ajoute une chaîne C à la fin
     * 
     * @param str Chaîne C terminée par null à ajouter
     * @return Référence vers this pour chaînage
     * 
     * @note Calcule automatiquement la longueur via string::NkLength().
     *       Gère str == nullptr comme chaîne vide (no-op).
     *       Garantit null-termination après ajout.
     */
    NkStringBuilder& NkStringBuilder::Append(const char* str) {
        if (!str) {
            return *this;
        }
        return Append(str, string::NkLength(str));
    }

    /**
     * @brief Ajoute N caractères depuis un buffer à la fin
     * 
     * @param str Pointeur vers les données à ajouter
     * @param length Nombre de caractères à copier depuis str
     * @return Référence vers this pour chaînage
     * 
     * @note Permet d'ajouter des sous-parties ou données non-terminées.
     *       Plus efficace que Append(str) si length < strlen(str).
     *       Utilise GrowIfNeeded() pour garantir la capacité suffisante.
     */
    NkStringBuilder& NkStringBuilder::Append(const char* str, SizeType length) {
        if (!str || length == 0) {
            return *this;
        }
        GrowIfNeeded(length);
        memory::NkMemCopy(mBuffer + mLength, str, length);
        mLength += length;
        mBuffer[mLength] = '\0';
        return *this;
    }

    /**
     * @brief Ajoute le contenu d'une autre NkString à la fin
     * 
     * @param str Instance NkString à concaténer
     * @return Référence vers this pour chaînage
     * 
     * @note Optimisé : évite conversion intermédiaire.
     *       Gère l'auto-concaténation correctement.
     */
    NkStringBuilder& NkStringBuilder::Append(const NkString& str) {
        return Append(str.Data(), str.Length());
    }

    /**
     * @brief Ajoute le contenu d'une NkStringView à la fin
     * 
     * @param view Vue de chaîne à concaténer
     * @return Référence vers this pour chaînage
     * 
     * @note Efficace : copie directe depuis la vue sans allocation temporaire.
     */
    NkStringBuilder& NkStringBuilder::Append(NkStringView view) {
        return Append(view.Data(), view.Length());
    }

    /**
     * @brief Ajoute un caractère unique à la fin
     * 
     * @param ch Caractère à ajouter
     * @return Référence vers this pour chaînage
     * 
     * @note Méthode la plus courante pour construction caractère par caractère.
     *       Très efficace : O(1) amorti grâce au buffer croissant.
     */
    NkStringBuilder& NkStringBuilder::Append(char ch) {
        GrowIfNeeded(1);
        mBuffer[mLength++] = ch;
        mBuffer[mLength] = '\0';
        return *this;
    }

    /**
     * @brief Ajoute N répétitions d'un caractère à la fin
     * 
     * @param count Nombre de fois répéter le caractère
     * @param ch Caractère à répéter
     * @return Référence vers this pour chaînage
     * 
     * @note Ex: sb.Append(3, '-') ajoute "---" à la fin.
     *       Utilise NkMemSet pour efficacité sur remplissage.
     */
    NkStringBuilder& NkStringBuilder::Append(SizeType count, char ch) {
        if (count == 0) {
            return *this;
        }
        GrowIfNeeded(count);
        memory::NkMemSet(mBuffer + mLength, ch, count);
        mLength += count;
        mBuffer[mLength] = '\0';
        return *this;
    }

    // =====================================================================
    // SECTION : OPÉRATIONS D'AJOUT - TYPES NUMÉRIQUES
    // =====================================================================

    /**
     * @brief Ajoute un entier signé 8 bits en représentation décimale
     * 
     * @param value Valeur à convertir et ajouter
     * @return Référence vers this pour chaînage
     * 
     * @note Délègue à Append(int32) après cast pour réutilisation.
     */
    NkStringBuilder& NkStringBuilder::Append(int8 value) {
        return Append(static_cast<int32>(value));
    }

    /**
     * @brief Ajoute un entier signé 16 bits en représentation décimale
     * 
     * @param value Valeur à convertir et ajouter
     * @return Référence vers this pour chaînage
     * 
     * @note Délègue à Append(int32) après cast pour réutilisation.
     */
    NkStringBuilder& NkStringBuilder::Append(int16 value) {
        return Append(static_cast<int32>(value));
    }

    /**
     * @brief Ajoute un entier signé 32 bits en représentation décimale
     * 
     * @param value Valeur à convertir et ajouter
     * @return Référence vers this pour chaînage
     * 
     * @note Utilise le buffer local de 32 chars pour conversion.
     *       Délègue à IntToString() puis Append(buffer, len).
     */
    NkStringBuilder& NkStringBuilder::Append(int32 value) {
        char buffer[32];
        SizeType len = IntToString(value, buffer, sizeof(buffer));
        return Append(buffer, len);
    }

    /**
     * @brief Ajoute un entier signé 64 bits en représentation décimale
     * 
     * @param value Valeur à convertir et ajouter
     * @return Référence vers this pour chaînage
     * 
     * @note Utilise le buffer local de 32 chars pour conversion.
     *       Délègue à IntToString() puis Append(buffer, len).
     */
    NkStringBuilder& NkStringBuilder::Append(int64 value) {
        char buffer[32];
        SizeType len = IntToString(value, buffer, sizeof(buffer));
        return Append(buffer, len);
    }

    /**
     * @brief Ajoute un entier non-signé 8 bits en représentation décimale
     * 
     * @param value Valeur à convertir et ajouter
     * @return Référence vers this pour chaînage
     * 
     * @note Délègue à Append(uint32) après cast pour réutilisation.
     */
    NkStringBuilder& NkStringBuilder::Append(uint8 value) {
        return Append(static_cast<uint32>(value));
    }

    /**
     * @brief Ajoute un entier non-signé 16 bits en représentation décimale
     * 
     * @param value Valeur à convertir et ajouter
     * @return Référence vers this pour chaînage
     * 
     * @note Délègue à Append(uint32) après cast pour réutilisation.
     */
    NkStringBuilder& NkStringBuilder::Append(uint16 value) {
        return Append(static_cast<uint32>(value));
    }

    /**
     * @brief Ajoute un entier non-signé 32 bits en représentation décimale
     * 
     * @param value Valeur à convertir et ajouter
     * @return Référence vers this pour chaînage
     * 
     * @note Utilise le buffer local de 32 chars pour conversion.
     *       Délègue à UIntToString() puis Append(buffer, len).
     */
    NkStringBuilder& NkStringBuilder::Append(uint32 value) {
        char buffer[32];
        SizeType len = UIntToString(value, buffer, sizeof(buffer));
        return Append(buffer, len);
    }

    /**
     * @brief Ajoute un entier non-signé 64 bits en représentation décimale
     * 
     * @param value Valeur à convertir et ajouter
     * @return Référence vers this pour chaînage
     * 
     * @note Utilise le buffer local de 32 chars pour conversion.
     *       Délègue à UIntToString() puis Append(buffer, len).
     */
    NkStringBuilder& NkStringBuilder::Append(uint64 value) {
        char buffer[32];
        SizeType len = UIntToString(value, buffer, sizeof(buffer));
        return Append(buffer, len);
    }

    /**
     * @brief Ajoute un flottant 32 bits en représentation décimale
     * 
     * @param value Valeur à convertir et ajouter
     * @return Référence vers this pour chaînage
     * 
     * @note Utilise le buffer local de 64 chars pour conversion.
     *       Précision par défaut : 6 décimales.
     *       Délègue à FloatToString() puis Append(buffer, len).
     */
    NkStringBuilder& NkStringBuilder::Append(float32 value) {
        char buffer[64];
        SizeType len = FloatToString(value, buffer, sizeof(buffer));
        return Append(buffer, len);
    }

    /**
     * @brief Ajoute un flottant 64 bits en représentation décimale
     * 
     * @param value Valeur à convertir et ajouter
     * @return Référence vers this pour chaînage
     * 
     * @note Utilise le buffer local de 64 chars pour conversion.
     *       Précision par défaut : 6 décimales.
     *       Délègue à FloatToString() puis Append(buffer, len).
     */
    NkStringBuilder& NkStringBuilder::Append(float64 value) {
        char buffer[64];
        SizeType len = FloatToString(value, buffer, sizeof(buffer));
        return Append(buffer, len);
    }

    /**
     * @brief Ajoute une valeur booléenne en texte
     * 
     * @param value Valeur booléenne à convertir
     * @return Référence vers this pour chaînage
     * 
     * @note Ajoute "true" ou "false" (minuscules, style JSON).
     */
    NkStringBuilder& NkStringBuilder::Append(bool value) {
        return Append(value ? "true" : "false");
    }

    // =====================================================================
    // SECTION : OPÉRATIONS D'AJOUT - REPRÉSENTATIONS ALTERNATIVES
    // =====================================================================

    /**
     * @brief Ajoute un entier non-signé 32 bits en hexadécimal
     * 
     * @param value Valeur à convertir
     * @param prefix true pour ajouter le préfixe "0x", false sinon
     * @return Référence vers this pour chaînage
     * 
     * @note Chiffres en majuscules : "FF", "A1B2", etc.
     *       Utilise le buffer local de 32 chars pour conversion.
     *       Délègue à HexToString() puis Append(buffer, len).
     */
    NkStringBuilder& NkStringBuilder::AppendHex(uint32 value, bool prefix) {
        char buffer[32];
        SizeType len = HexToString(value, buffer, sizeof(buffer), prefix);
        return Append(buffer, len);
    }

    /**
     * @brief Ajoute un entier non-signé 64 bits en hexadécimal
     * 
     * @param value Valeur à convertir
     * @param prefix true pour ajouter le préfixe "0x", false sinon
     * @return Référence vers this pour chaînage
     * 
     * @note Chiffres en majuscules : "FF", "A1B2", etc.
     *       Utilise le buffer local de 32 chars pour conversion.
     *       Délègue à HexToString() puis Append(buffer, len).
     */
    NkStringBuilder& NkStringBuilder::AppendHex(uint64 value, bool prefix) {
        char buffer[32];
        SizeType len = HexToString(value, buffer, sizeof(buffer), prefix);
        return Append(buffer, len);
    }

    /**
     * @brief Ajoute un entier non-signé 32 bits en binaire
     * 
     * @param value Valeur à convertir
     * @param bits Nombre de bits à afficher (1-32, défaut: 32)
     * @return Référence vers this pour chaînage
     * 
     * @note Utilise le buffer local de 128 chars (32 bits + espaces).
     *       Padding avec zéros à gauche si bits > largeur réelle.
     *       Ajoute des espaces tous les 4 bits pour lisibilité.
     */
    NkStringBuilder& NkStringBuilder::AppendBinary(uint32 value, SizeType bits) {
        char buffer[128];
        SizeType len = BinaryToString(value, buffer, sizeof(buffer), bits);
        return Append(buffer, len);
    }

    /**
     * @brief Ajoute un entier non-signé 64 bits en binaire
     * 
     * @param value Valeur à convertir
     * @param bits Nombre de bits à afficher (1-64, défaut: 64)
     * @return Référence vers this pour chaînage
     * 
     * @note Utilise le buffer local de 256 chars (64 bits + espaces).
     *       Padding avec zéros à gauche si bits > largeur réelle.
     *       Ajoute des espaces tous les 4 bits pour lisibilité.
     */
    NkStringBuilder& NkStringBuilder::AppendBinary(uint64 value, SizeType bits) {
        char buffer[256];
        SizeType len = BinaryToString(value, buffer, sizeof(buffer), bits);
        return Append(buffer, len);
    }

    /**
     * @brief Ajoute un entier non-signé 32 bits en octal
     * 
     * @param value Valeur à convertir
     * @param prefix true pour ajouter le préfixe "0", false sinon
     * @return Référence vers this pour chaînage
     * 
     * @note Utilise le buffer local de 32 chars pour conversion.
     *       Délègue à OctalToString() puis Append(buffer, len).
     */
    NkStringBuilder& NkStringBuilder::AppendOctal(uint32 value, bool prefix) {
        char buffer[32];
        SizeType len = OctalToString(value, buffer, sizeof(buffer), prefix);
        return Append(buffer, len);
    }

    /**
     * @brief Ajoute un entier non-signé 64 bits en octal
     * 
     * @param value Valeur à convertir
     * @param prefix true pour ajouter le préfixe "0", false sinon
     * @return Référence vers this pour chaînage
     * 
     * @note Utilise le buffer local de 32 chars pour conversion.
     *       Délègue à OctalToString() puis Append(buffer, len).
     */
    NkStringBuilder& NkStringBuilder::AppendOctal(uint64 value, bool prefix) {
        char buffer[32];
        SizeType len = OctalToString(value, buffer, sizeof(buffer), prefix);
        return Append(buffer, len);
    }

    // =====================================================================
    // SECTION : OPÉRATEUR DE FLUX STYLE (<<)
    // =====================================================================

    /**
     * @brief Opérateur << pour chaîne C
     * 
     * @param str Chaîne C à ajouter
     * @return Référence vers this pour chaînage
     * 
     * @note Alias pour Append(str). Permet syntaxe style flux :
     *       sb << "Hello" << " " << "World";
     */
    NkStringBuilder& NkStringBuilder::operator<<(const char* str) { 
        return Append(str); 
    }

    /**
     * @brief Opérateur << pour NkString
     * 
     * @param str Instance NkString à ajouter
     * @return Référence vers this pour chaînage
     * 
     * @note Alias pour Append(str).
     */
    NkStringBuilder& NkStringBuilder::operator<<(const NkString& str) { 
        return Append(str); 
    }

    /**
     * @brief Opérateur << pour NkStringView
     * 
     * @param view Vue de chaîne à ajouter
     * @return Référence vers this pour chaînage
     * 
     * @note Alias pour Append(view).
     */
    NkStringBuilder& NkStringBuilder::operator<<(NkStringView view) { 
        return Append(view); 
    }

    /**
     * @brief Opérateur << pour caractère unique
     * 
     * @param ch Caractère à ajouter
     * @return Référence vers this pour chaînage
     * 
     * @note Alias pour Append(ch).
     */
    NkStringBuilder& NkStringBuilder::operator<<(char ch) { 
        return Append(ch); 
    }

    /**
     * @brief Opérateur << pour entier signé 8 bits
     * 
     * @param value Valeur à ajouter
     * @return Référence vers this pour chaînage
     * 
     * @note Alias pour Append(value).
     */
    NkStringBuilder& NkStringBuilder::operator<<(int8 value) { 
        return Append(value); 
    }

    /**
     * @brief Opérateur << pour entier signé 16 bits
     * 
     * @param value Valeur à ajouter
     * @return Référence vers this pour chaînage
     */
    NkStringBuilder& NkStringBuilder::operator<<(int16 value) { 
        return Append(value); 
    }

    /**
     * @brief Opérateur << pour entier signé 32 bits
     * 
     * @param value Valeur à ajouter
     * @return Référence vers this pour chaînage
     */
    NkStringBuilder& NkStringBuilder::operator<<(int32 value) { 
        return Append(value); 
    }

    /**
     * @brief Opérateur << pour entier signé 64 bits
     * 
     * @param value Valeur à ajouter
     * @return Référence vers this pour chaînage
     */
    NkStringBuilder& NkStringBuilder::operator<<(int64 value) { 
        return Append(value); 
    }

    /**
     * @brief Opérateur << pour entier non-signé 8 bits
     * 
     * @param value Valeur à ajouter
     * @return Référence vers this pour chaînage
     */
    NkStringBuilder& NkStringBuilder::operator<<(uint8 value) { 
        return Append(value); 
    }

    /**
     * @brief Opérateur << pour entier non-signé 16 bits
     * 
     * @param value Valeur à ajouter
     * @return Référence vers this pour chaînage
     */
    NkStringBuilder& NkStringBuilder::operator<<(uint16 value) { 
        return Append(value); 
    }

    /**
     * @brief Opérateur << pour entier non-signé 32 bits
     * 
     * @param value Valeur à ajouter
     * @return Référence vers this pour chaînage
     */
    NkStringBuilder& NkStringBuilder::operator<<(uint32 value) { 
        return Append(value); 
    }

    /**
     * @brief Opérateur << pour entier non-signé 64 bits
     * 
     * @param value Valeur à ajouter
     * @return Référence vers this pour chaînage
     */
    NkStringBuilder& NkStringBuilder::operator<<(uint64 value) { 
        return Append(value); 
    }

    /**
     * @brief Opérateur << pour flottant 32 bits
     * 
     * @param value Valeur à ajouter
     * @return Référence vers this pour chaînage
     */
    NkStringBuilder& NkStringBuilder::operator<<(float32 value) { 
        return Append(value); 
    }

    /**
     * @brief Opérateur << pour flottant 64 bits
     * 
     * @param value Valeur à ajouter
     * @return Référence vers this pour chaînage
     */
    NkStringBuilder& NkStringBuilder::operator<<(float64 value) { 
        return Append(value); 
    }

    /**
     * @brief Opérateur << pour booléen
     * 
     * @param value Valeur booléenne à ajouter
     * @return Référence vers this pour chaînage
     * 
     * @note Ajoute "true" ou "false".
     */
    NkStringBuilder& NkStringBuilder::operator<<(bool value) { 
        return Append(value); 
    }

    // =====================================================================
    // SECTION : OPÉRATIONS D'AJOUT DE LIGNES ET FORMATAGE
    // =====================================================================

    /**
     * @brief Ajoute un saut de ligne (newline)
     * 
     * @return Référence vers this pour chaînage
     * 
     * @note Ajoute '\n' (Unix-style).
     *       Pour Windows-style, utiliser Append("\r\n").
     */
    NkStringBuilder& NkStringBuilder::AppendLine() {
        return Append('\n');
    }

    /**
     * @brief Ajoute une chaîne suivie d'un saut de ligne
     * 
     * @param str Vue de chaîne à ajouter avant le newline
     * @return Référence vers this pour chaînage
     * 
     * @note Équivalent à Append(str).AppendLine().
     */
    NkStringBuilder& NkStringBuilder::AppendLine(NkStringView str) {
        Append(str);
        return Append('\n');
    }

    /**
     * @brief Ajoute une chaîne C suivie d'un saut de ligne
     * 
     * @param str Chaîne C à ajouter avant le newline
     * @return Référence vers this pour chaînage
     * 
     * @note Équivalent à Append(str).AppendLine().
     */
    NkStringBuilder& NkStringBuilder::AppendLine(const char* str) {
        Append(str);
        return Append('\n');
    }

    /**
     * @brief Ajoute une NkString suivie d'un saut de ligne
     * 
     * @param str Instance NkString à ajouter avant le newline
     * @return Référence vers this pour chaînage
     * 
     * @note Équivalent à Append(str).AppendLine().
     */
    NkStringBuilder& NkStringBuilder::AppendLine(const NkString& str) {
        Append(str);
        return Append('\n');
    }

    /**
     * @brief Ajoute une chaîne formatée style printf
     * 
     * @param format Chaîne de format avec spécificateurs (%s, %d, %.2f, etc.)
     * @param ... Arguments variables correspondant aux spécificateurs
     * @return Référence vers this pour chaînage
     * 
     * @note Syntaxe compatible vsnprintf.
     *       Délègue à AppendVFormat() après gestion va_list.
     */
    NkStringBuilder& NkStringBuilder::AppendFormat(const char* format, ...) {
        if (!format) {
            return *this;
        }
        va_list args;
        va_start(args, format);
        AppendVFormat(format, args);
        va_end(args);
        return *this;
    }

    /**
     * @brief Ajoute une chaîne formatée avec va_list
     * 
     * @param format Chaîne de format avec spécificateurs
     * @param args Liste d'arguments de type va_list
     * @return Référence vers this pour chaînage
     * 
     * @note Calcule d'abord la taille nécessaire avec vsnprintf(nullptr, 0, ...).
     *       Puis alloue la capacité requise et formate dans le buffer.
     *       Ne modifie pas args : peut être réutilisé après appel.
     */
    NkStringBuilder& NkStringBuilder::AppendVFormat(const char* format, va_list args) {
        if (!format) {
            return *this;
        }
        va_list args_copy;
        va_copy(args_copy, args);
        int size = std::vsnprintf(nullptr, 0, format, args_copy);
        va_end(args_copy);
        if (size < 0) {
            return *this;
        }
        GrowIfNeeded(size);
        std::vsnprintf(mBuffer + mLength, size + 1, format, args);
        mLength += size;
        return *this;
    }

    // =====================================================================
    // SECTION : OPÉRATIONS D'INSERTION À POSITION
    // =====================================================================

    /**
     * @brief Insère un caractère à une position donnée
     * 
     * @param pos Index où insérer le nouveau caractère (0 = début)
     * @param ch Caractère à insérer
     * @return Référence vers this pour chaînage
     * 
     * @note Décale les caractères existants vers la droite via NkMemMove.
     *       Assertion si pos > Length().
     *       Garantit null-termination après insertion.
     */
    NkStringBuilder& NkStringBuilder::Insert(SizeType pos, char ch) {
        NKENTSEU_ASSERT(pos <= mLength);
        GrowIfNeeded(1);
        memory::NkMemMove(mBuffer + pos + 1, mBuffer + pos, mLength - pos);
        mBuffer[pos] = ch;
        ++mLength;
        mBuffer[mLength] = '\0';
        return *this;
    }

    /**
     * @brief Insère une chaîne C à une position donnée
     * 
     * @param pos Index d'insertion
     * @param str Chaîne C à insérer
     * @return Référence vers this pour chaînage
     * 
     * @note Délègue à Insert(pos, str, string::NkLength(str)).
     */
    NkStringBuilder& NkStringBuilder::Insert(SizeType pos, const char* str) {
        if (!str) {
            return *this;
        }
        return Insert(pos, str, string::NkLength(str));
    }

    /**
     * @brief Insère N caractères depuis un buffer à une position
     * 
     * @param pos Index d'insertion
     * @param str Pointeur vers les données à insérer
     * @param length Nombre de caractères à copier depuis str
     * @return Référence vers this pour chaînage
     * 
     * @note Permet d'insérer des sous-parties ou données non-terminées.
     *       Utilise NkMemMove pour décaler les données existantes.
     */
    NkStringBuilder& NkStringBuilder::Insert(
        SizeType pos, 
        const char* str, 
        SizeType length) {
        
        NKENTSEU_ASSERT(pos <= mLength);
        if (!str || length == 0) {
            return *this;
        }
        GrowIfNeeded(length);
        memory::NkMemMove(mBuffer + pos + length, mBuffer + pos, mLength - pos);
        memory::NkMemCopy(mBuffer + pos, str, length);
        mLength += length;
        mBuffer[mLength] = '\0';
        return *this;
    }

    /**
     * @brief Insère une NkStringView à une position donnée
     * 
     * @param pos Index d'insertion
     * @param view Vue de chaîne à insérer
     * @return Référence vers this pour chaînage
     * 
     * @note Efficace : copie directe depuis la vue.
     */
    NkStringBuilder& NkStringBuilder::Insert(SizeType pos, NkStringView view) {
        return Insert(pos, view.Data(), view.Length());
    }

    /**
     * @brief Insère une NkString à une position donnée
     * 
     * @param pos Index d'insertion
     * @param str Instance NkString à insérer
     * @return Référence vers this pour chaînage
     * 
     * @note Délègue à Insert(pos, str.Data(), str.Length()).
     */
    NkStringBuilder& NkStringBuilder::Insert(SizeType pos, const NkString& str) {
        return Insert(pos, str.Data(), str.Length());
    }

    /**
     * @brief Insère N répétitions d'un caractère à une position
     * 
     * @param pos Index d'insertion
     * @param count Nombre de répétitions du caractère
     * @param ch Caractère à insérer
     * @return Référence vers this pour chaînage
     * 
     * @note Ex: sb.Insert(2, 3, '*') sur "abcd" donne "ab***cd"
     *       Utilise NkMemSet pour efficacité sur remplissage.
     */
    NkStringBuilder& NkStringBuilder::Insert(
        SizeType pos, 
        SizeType count, 
        char ch) {
        
        NKENTSEU_ASSERT(pos <= mLength);
        if (count == 0) {
            return *this;
        }
        GrowIfNeeded(count);
        memory::NkMemMove(mBuffer + pos + count, mBuffer + pos, mLength - pos);
        memory::NkMemSet(mBuffer + pos, ch, count);
        mLength += count;
        mBuffer[mLength] = '\0';
        return *this;
    }

    // =====================================================================
    // SECTION : OPÉRATIONS DE REMPLACEMENT
    // =====================================================================

    /**
     * @brief Remplace une plage par une chaîne C
     * 
     * @param pos Index de début de la plage à remplacer
     * @param count Nombre de caractères à remplacer
     * @param str Chaîne C de remplacement
     * @return Référence vers this pour chaînage
     * 
     * @note Si str est nullptr : équivalent à Remove(pos, count).
     *       Gère la différence de taille entre l'ancienne et la nouvelle chaîne.
     *       Utilise NkMemMove pour décaler les données après la plage.
     */
    NkStringBuilder& NkStringBuilder::Replace(
        SizeType pos, 
        SizeType count, 
        const char* str) {
        
        NKENTSEU_ASSERT(pos <= mLength);
        if (!str) {
            return Remove(pos, count);
        }
        SizeType strLen = string::NkLength(str);
        SizeType newLength = mLength - count + strLen;
        if (strLen > count) {
            GrowIfNeeded(strLen - count);
        }
        memory::NkMemMove(
            mBuffer + pos + strLen, 
            mBuffer + pos + count, 
            mLength - pos - count);
        memory::NkMemCopy(mBuffer + pos, str, strLen);
        mLength = newLength;
        mBuffer[mLength] = '\0';
        return *this;
    }

    /**
     * @brief Remplace une plage par une NkStringView
     * 
     * @param pos Index de début de la plage
     * @param count Nombre de caractères à remplacer
     * @param view Vue de chaîne de remplacement
     * @return Référence vers this pour chaînage
     * 
     * @note Délègue à Replace(pos, count, view.Data()).
     */
    NkStringBuilder& NkStringBuilder::Replace(
        SizeType pos, 
        SizeType count, 
        NkStringView view) {
        return Replace(pos, count, view.Data());
    }

    /**
     * @brief Remplace la première occurrence d'un caractère par un autre
     * 
     * @param oldChar Caractère à rechercher
     * @param newChar Caractère de remplacement
     * @return Référence vers this pour chaînage
     * 
     * @note Parcours linéaire : s'arrête à la première occurrence trouvée.
     *       Ex: sb.Replace('a', 'A') sur "banana" donne "bAnana"
     */
    NkStringBuilder& NkStringBuilder::Replace(char oldChar, char newChar) {
        for (SizeType i = 0; i < mLength; ++i) {
            if (mBuffer[i] == oldChar) {
                mBuffer[i] = newChar;
                return *this;
            }
        }
        return *this;
    }

    /**
     * @brief Remplace la première occurrence d'une sous-chaîne
     * 
     * @param oldStr Sous-chaîne à rechercher
     * @param newStr Chaîne de remplacement
     * @return Référence vers this pour chaînage
     * 
     * @note Utilise Find() pour localiser la première occurrence.
     *       Si trouvée : appelle Replace(pos, oldStr.Length(), newStr).
     */
    NkStringBuilder& NkStringBuilder::Replace(
        NkStringView oldStr, 
        NkStringView newStr) {
        
        SizeType pos = Find(oldStr);
        if (pos != NkStringView::npos) {
            Replace(pos, oldStr.Length(), newStr);
        }
        return *this;
    }

    /**
     * @brief Remplace toutes les occurrences d'un caractère par un autre
     * 
     * @param oldChar Caractère à rechercher
     * @param newChar Caractère de remplacement
     * @return Référence vers this pour chaînage
     * 
     * @note Parcours complet du buffer : O(n).
     *       Ex: sb.ReplaceAll('a', 'A') sur "banana" donne "bAnAnA"
     */
    NkStringBuilder& NkStringBuilder::ReplaceAll(char oldChar, char newChar) {
        for (SizeType i = 0; i < mLength; ++i) {
            if (mBuffer[i] == oldChar) {
                mBuffer[i] = newChar;
            }
        }
        return *this;
    }

    /**
     * @brief Remplace toutes les occurrences d'une sous-chaîne
     * 
     * @param oldStr Sous-chaîne à rechercher
     * @param newStr Chaîne de remplacement
     * @return Référence vers this pour chaînage
     * 
     * @note Parcours séquentiel avec mise à jour de la position de recherche.
     *       Les remplacements ne se chevauchent pas.
     *       Ex: sb.ReplaceAll("aa", "b") sur "aaa" donne "ba" (pas "bbb")
     */
    NkStringBuilder& NkStringBuilder::ReplaceAll(
        NkStringView oldStr, 
        NkStringView newStr) {
        
        if (oldStr.Empty()) {
            return *this;
        }
        SizeType oldLen = oldStr.Length();
        SizeType newLen = newStr.Length();
        SizeType pos = 0;
        while ((pos = Find(oldStr, pos)) != NkStringView::npos) {
            Replace(pos, oldLen, newStr);
            pos += newLen;
        }
        return *this;
    }

    // =====================================================================
    // SECTION : OPÉRATIONS DE SUPPRESSION ET NETTOYAGE
    // =====================================================================

    /**
     * @brief Supprime une plage de caractères
     * 
     * @param pos Index de début de la plage à supprimer
     * @param count Nombre de caractères à supprimer
     * @return Référence vers this pour chaînage
     * 
     * @note Décale les caractères suivants vers la gauche via NkMemMove.
     *       Ne réduit pas automatiquement la capacité.
     *       Garantit null-termination après suppression.
     */
    NkStringBuilder& NkStringBuilder::Remove(SizeType pos, SizeType count) {
        NKENTSEU_ASSERT(pos <= mLength);
        if (pos + count > mLength) {
            count = mLength - pos;
        }
        memory::NkMemMove(mBuffer + pos, mBuffer + pos + count, mLength - pos - count);
        mLength -= count;
        mBuffer[mLength] = '\0';
        return *this;
    }

    /**
     * @brief Supprime le caractère à une position donnée
     * 
     * @param pos Index du caractère à supprimer
     * @return Référence vers this pour chaînage
     * 
     * @note Équivalent à Remove(pos, 1).
     */
    NkStringBuilder& NkStringBuilder::RemoveAt(SizeType pos) {
        return Remove(pos, 1);
    }

    /**
     * @brief Supprime N caractères au début de la chaîne
     * 
     * @param count Nombre de caractères à retirer du préfixe
     * @return Référence vers this pour chaînage
     * 
     * @note Équivalent à Remove(0, count).
     */
    NkStringBuilder& NkStringBuilder::RemovePrefix(SizeType count) {
        return Remove(0, count);
    }

    /**
     * @brief Supprime N caractères à la fin de la chaîne
     * 
     * @param count Nombre de caractères à retirer du suffixe
     * @return Référence vers this pour chaînage
     * 
     * @note Équivalent à Remove(Length() - count, count).
     */
    NkStringBuilder& NkStringBuilder::RemoveSuffix(SizeType count) {
        if (count > mLength) {
            count = mLength;
        }
        return Remove(mLength - count, count);
    }

    /**
     * @brief Supprime toutes les occurrences d'un caractère
     * 
     * @param ch Caractère à supprimer
     * @return Référence vers this pour chaînage
     * 
     * @note Compactage in-place : O(n).
     *       Utilise deux pointeurs (lecture/écriture) pour éviter allocations.
     *       Ex: sb.RemoveAll('a') sur "banana" donne "bnn"
     */
    NkStringBuilder& NkStringBuilder::RemoveAll(char ch) {
        SizeType writePos = 0;
        for (SizeType readPos = 0; readPos < mLength; ++readPos) {
            if (mBuffer[readPos] != ch) {
                mBuffer[writePos++] = mBuffer[readPos];
            }
        }
        mLength = writePos;
        mBuffer[mLength] = '\0';
        return *this;
    }

    /**
     * @brief Supprime toutes les occurrences d'une sous-chaîne
     * 
     * @param str Sous-chaîne à supprimer
     * @return Référence vers this pour chaînage
     * 
     * @note Parcours séquentiel avec compactage.
     *       Utilise NkMemCompare pour comparer les sous-chaînes.
     */
    NkStringBuilder& NkStringBuilder::RemoveAll(NkStringView str) {
        if (str.Empty()) {
            return *this;
        }
        SizeType strLen = str.Length();
        SizeType writePos = 0;
        SizeType readPos = 0;
        while (readPos < mLength) {
            if (readPos + strLen <= mLength && 
                memory::NkMemCompare(mBuffer + readPos, str.Data(), strLen) == 0) {
                readPos += strLen;
            } else {
                mBuffer[writePos++] = mBuffer[readPos++];
            }
        }
        mLength = writePos;
        mBuffer[mLength] = '\0';
        return *this;
    }

    /**
     * @brief Supprime tous les caractères d'espacement (whitespace)
     * 
     * @return Référence vers this pour chaînage
     * 
     * @note Supprime : ' ', '\t', '\n', '\r', '\v', '\f'
     *       Ex: "a b\tc\nd" -> "abcd"
     *       Utilise string::NkIsWhitespace() pour la détection.
     */
    NkStringBuilder& NkStringBuilder::RemoveWhitespace() {
        SizeType writePos = 0;
        for (SizeType readPos = 0; readPos < mLength; ++readPos) {
            if (!string::NkIsWhitespace(mBuffer[readPos])) {
                mBuffer[writePos++] = mBuffer[readPos];
            }
        }
        mLength = writePos;
        mBuffer[mLength] = '\0';
        return *this;
    }

    /**
     * @brief Supprime les espaces blancs aux deux extrémités
     * 
     * @return Référence vers this pour chaînage
     * 
     * @note Combine TrimLeft() + TrimRight().
     */
    NkStringBuilder& NkStringBuilder::Trim() {
        TrimLeft();
        TrimRight();
        return *this;
    }

    /**
     * @brief Supprime les espaces blancs à gauche uniquement
     * 
     * @return Référence vers this pour chaînage
     * 
     * @note Décale les données vers la gauche via RemovePrefix().
     */
    NkStringBuilder& NkStringBuilder::TrimLeft() {
        SizeType pos = 0;
        while (pos < mLength && string::NkIsWhitespace(mBuffer[pos])) {
            ++pos;
        }
        if (pos > 0) {
            RemovePrefix(pos);
        }
        return *this;
    }

    /**
     * @brief Supprime les espaces blancs à droite uniquement
     * 
     * @return Référence vers this pour chaînage
     * 
     * @note Ajuste simplement mLength : O(1).
     */
    NkStringBuilder& NkStringBuilder::TrimRight() {
        SizeType pos = mLength;
        while (pos > 0 && string::NkIsWhitespace(mBuffer[pos - 1])) {
            --pos;
        }
        if (pos < mLength) {
            mLength = pos;
            mBuffer[mLength] = '\0';
        }
        return *this;
    }

    /**
     * @brief Vide le builder sans libérer la capacité
     * 
     * @note Met Length() à 0, préserve Capacity() pour réutilisation.
     *       Aucune libération mémoire : efficace pour réutilisation.
     *       Le null-terminator est maintenu en position 0.
     */
    void NkStringBuilder::Clear() noexcept {
        mLength = 0;
        if (mBuffer) {
            mBuffer[0] = '\0';
        }
    }

    /**
     * @brief Réinitialise le builder (alias pour Clear)
     * 
     * @note Identique fonctionnellement à Clear().
     *       Nom alternatif pour sémantique de "reset".
     */
    void NkStringBuilder::Reset() noexcept {
        Clear();
    }

    /**
     * @brief Libère toute la mémoire allouée
     * 
     * @note Réinitialise à l'état par défaut : buffer=nullptr, length=0, capacity=0.
     *       Utile pour libérer de la mémoire après un gros build.
     */
    void NkStringBuilder::Release() {
        FreeBuffer();
        mLength = 0;
        mCapacity = 0;
    }

    // =====================================================================
    // SECTION : CAPACITÉ, TAILLE ET ÉTAT
    // =====================================================================

    /**
     * @brief Retourne le nombre de caractères dans le builder
     * 
     * @return Longueur actuelle en caractères (exclut null-terminator)
     * 
     * @note Complexité O(1) : valeur pré-calculée et maintenue.
     */
    NkStringBuilder::SizeType NkStringBuilder::Length() const noexcept {
        return mLength;
    }

    /**
     * @brief Alias pour Length() (convention STL)
     * 
     * @return Nombre d'éléments dans le builder
     */
    NkStringBuilder::SizeType NkStringBuilder::Size() const noexcept {
        return mLength;
    }

    /**
     * @brief Alias pour Length() avec cast explicite usize
     * 
     * @return Longueur castée en type usize standard
     */
    usize NkStringBuilder::Count() const noexcept {
        return static_cast<usize>(mLength);
    }

    /**
     * @brief Retourne la capacité totale allouée
     * 
     * @return Nombre maximal de caractères stockables sans réallocation
     * 
     * @note Capacity() >= Length() toujours.
     *       La différence représente l'espace de croissance réservé.
     */
    NkStringBuilder::SizeType NkStringBuilder::Capacity() const noexcept {
        return mCapacity;
    }

    /**
     * @brief Retourne la taille maximale théorique
     * 
     * @return MAX_CAPACITY : valeur limite avant overflow
     */
    NkStringBuilder::SizeType NkStringBuilder::MaxSize() const noexcept {
        return MAX_CAPACITY;
    }

    /**
     * @brief Teste si le builder est vide
     * 
     * @return true si Length() == 0, false sinon
     */
    bool NkStringBuilder::Empty() const noexcept {
        return mLength == 0;
    }

    /**
     * @brief Teste si le buffer interne est nullptr
     * 
     * @return true si mBuffer == nullptr, false sinon
     * 
     * @note Un builder peut être non-null mais vide (buffer valide, longueur 0).
     */
    bool NkStringBuilder::IsNull() const noexcept {
        return mBuffer == nullptr;
    }

    /**
     * @brief Teste si le builder est nullptr OU vide
     * 
     * @return true si mBuffer == nullptr OU mLength == 0
     * 
     * @note Méthode pratique pour les vérifications de validité rapides.
     */
    bool NkStringBuilder::IsNullOrEmpty() const noexcept {
        return mBuffer == nullptr || mLength == 0;
    }

    /**
     * @brief Teste si le buffer est plein (Length() == Capacity())
     * 
     * @return true si aucune croissance possible sans réallocation
     * 
     * @note Utile pour optimiser les appels à Reserve() avant boucles.
     */
    bool NkStringBuilder::IsFull() const noexcept {
        return mLength == mCapacity;
    }

    /**
     * @brief Réserve de la capacité pour croissance future
     * 
     * @param newCapacity Nouvelle capacité minimale souhaitée
     * 
     * @note Si newCapacity <= Capacity(), aucune action.
     *       Sinon : alloue un nouveau buffer et copie les données.
     *       Utile pour éviter les réallocations multiples en boucle.
     */
    void NkStringBuilder::Reserve(SizeType newCapacity) {
        if (newCapacity <= mCapacity) {
            return;
        }
        Reallocate(newCapacity);
    }

    /**
     * @brief Redimensionne le builder à une longueur cible
     * 
     * @param newSize Nouvelle longueur souhaitée
     * @param fillChar Caractère à utiliser pour l'extension (défaut: '\\0')
     * 
     * @note Si newSize < Length() : tronque à newSize caractères.
     *       Si newSize > Length() : étend avec fillChar.
     *       Peut déclencher réallocation si newSize > Capacity().
     */
    void NkStringBuilder::Resize(SizeType newSize, char fillChar) {
        if (newSize > mLength) {
            Reserve(newSize);
            memory::NkMemSet(mBuffer + mLength, fillChar, newSize - mLength);
            mLength = newSize;
            mBuffer[mLength] = '\0';
        } else if (newSize < mLength) {
            mLength = newSize;
            mBuffer[mLength] = '\0';
        }
    }

    /**
     * @brief Réduit la capacité au strict nécessaire
     * 
     * @note Si Length() < Capacity() : réalloue à taille exacte.
     *       Peut déclencher une réallocation et copie.
     *       Utile après construction finale pour minimiser l'empreinte mémoire.
     */
    void NkStringBuilder::ShrinkToFit() {
        if (mLength == mCapacity) {
            return;
        }
        char* newBuffer = static_cast<char*>(
            mAllocator->Allocate((mLength + 1) * sizeof(char)));
        if (mBuffer) {
            if (mLength > 0) {
                memory::NkMemCopy(newBuffer, mBuffer, mLength);
            }
            mAllocator->Deallocate(mBuffer);
        }
        mBuffer = newBuffer;
        mCapacity = mLength;
        mBuffer[mLength] = '\0';
    }

    // =====================================================================
    // SECTION : ACCÈS AUX ÉLÉMENTS
    // =====================================================================

    /**
     * @brief Accès indexé sans vérification de bornes (version modifiable)
     * 
     * @param index Position du caractère à accéder
     * @return Référence modifiable vers le caractère
     * 
     * @warning Comportement indéfini si index >= Length().
     *          Utiliser At() en mode développement pour sécurité.
     */
    char& NkStringBuilder::operator[](SizeType index) {
        NKENTSEU_ASSERT(index < mLength);
        return mBuffer[index];
    }

    /**
     * @brief Accès indexé sans vérification de bornes (version constante)
     * 
     * @param index Position du caractère à accéder
     * @return Référence constante vers le caractère
     */
    const char& NkStringBuilder::operator[](SizeType index) const {
        NKENTSEU_ASSERT(index < mLength);
        return mBuffer[index];
    }

    /**
     * @brief Accès indexé avec vérification de bornes (version modifiable)
     * 
     * @param index Position du caractère à accéder
     * @return Référence modifiable vers le caractère
     * 
     * @note Lève une assertion avec message en mode debug si index hors bornes.
     *       Préférer cette méthode pour code robuste en développement.
     */
    char& NkStringBuilder::At(SizeType index) {
        NKENTSEU_ASSERT(index < mLength);
        return mBuffer[index];
    }

    /**
     * @brief Accès indexé avec vérification de bornes (version constante)
     * 
     * @param index Position du caractère à accéder
     * @return Référence constante vers le caractère
     */
    const char& NkStringBuilder::At(SizeType index) const {
        NKENTSEU_ASSERT(index < mLength);
        return mBuffer[index];
    }

    /**
     * @brief Accès au premier caractère (version modifiable)
     *
     * @return Référence modifiable vers le caractère en position 0
     */
    char& NkStringBuilder::Front() {
        NKENTSEU_ASSERT(mLength > 0);
        return mBuffer[mLength - 1];
    }

    /**
     * @brief Accès au premier caractère (version constante)
     * 
     * @return Référence constante vers le caractère en position 0
     */
    const char& NkStringBuilder::Front() const {
        NKENTSEU_ASSERT(mLength > 0);
        return mBuffer[0];
    }

    /**
     * @brief Accès au dernier caractère (version modifiable)
     * 
     * @return Référence modifiable vers le dernier caractère
     * 
     * @warning Assertion si Empty() : vérifier avant appel.
     */
    char& NkStringBuilder::Back() {
        NKENTSEU_ASSERT(mLength > 0);
        return mBuffer[mLength - 1];
    }

    /**
     * @brief Accès au dernier caractère (version constante)
     * 
     * @return Référence constante vers le dernier caractère
     */
    const char& NkStringBuilder::Back() const {
        NKENTSEU_ASSERT(mLength > 0);
        return mBuffer[mLength - 1];
    }

    /**
     * @brief Accès aux données brutes (version modifiable)
     * 
     * @return Pointeur modifiable vers le premier caractère
     * 
     * @warning Modifier les données via ce pointeur peut invalider
     *          les métadonnées internes (longueur, null-terminator).
     *          Utiliser avec précaution, préférer les méthodes API.
     */
    char* NkStringBuilder::Data() noexcept {
        return mBuffer;
    }

    /**
     * @brief Accès aux données brutes (version constante)
     * 
     * @return Pointeur constant vers le premier caractère
     * 
     * @note Retourne toujours un pointeur valide, même si Empty().
     *       Les données sont garanties null-terminated.
     */
    const char* NkStringBuilder::Data() const noexcept {
        return mBuffer;
    }

    /**
     * @brief Accès compatible C-string (null-terminated garanti)
     * 
     * @return Pointeur constant vers chaîne C-style
     * 
     * @note Méthode privilégiée pour interop avec APIs C.
     *       Le pointeur reste valide jusqu'à la prochaine
     *       modification non-const de cette instance.
     */
    const char* NkStringBuilder::CStr() const noexcept {
        return mBuffer;
    }

    // =====================================================================
    // SECTION : EXTRACTION DE SOUS-CHAÎNES (RETOURNE DES VUES)
    // =====================================================================

    /**
     * @brief Extrait une sous-vue à partir d'une position
     * 
     * @param pos Position de départ dans le builder courant
     * @param count Nombre maximal de caractères à inclure (npos = jusqu'à la fin)
     * @return Nouvelle NkStringView sur la sous-partie spécifiée
     * 
     * @note Aucune copie : la vue référence les données internes du builder.
     *       @warning La vue devient invalide si le builder est modifié.
     *       Assertion si pos > Length().
     */
    NkStringView NkStringBuilder::SubStr(
        SizeType pos, 
        SizeType count) const {
        
        NKENTSEU_ASSERT(pos <= mLength);
        SizeType rcount = (count == NkStringView::npos || pos + count > mLength) 
            ? mLength - pos 
            : count;
        return NkStringView(mBuffer + pos, rcount);
    }

    /**
     * @brief Extrait une sous-vue par bornes [start, end)
     * 
     * @param start Index inclus du début de la sous-vue
     * @param end Index exclu de la fin de la sous-vue
     * @return Nouvelle NkStringView sur l'intervalle spécifié
     * 
     * @note Interface style "slice" Python : plus intuitive pour certains cas.
     *       Aucune copie : attention à la durée de vie du builder.
     */
    NkStringView NkStringBuilder::Slice(SizeType start, SizeType end) const {
        NKENTSEU_ASSERT(start <= end && end <= mLength);
        return NkStringView(mBuffer + start, end - start);
    }

    /**
     * @brief Retourne une vue sur les N premiers caractères
     * 
     * @param count Nombre de caractères à extraire du début
     * @return Vue sur le préfixe de longueur min(count, Length())
     */
    NkStringView NkStringBuilder::Left(SizeType count) const {
        return SubStr(0, count);
    }

    /**
     * @brief Retourne une vue sur les N derniers caractères
     * 
     * @param count Nombre de caractères à extraire de la fin
     * @return Vue sur le suffixe de longueur min(count, Length())
     */
    NkStringView NkStringBuilder::Right(SizeType count) const {
        if (count >= mLength) {
            return NkStringView(mBuffer, mLength);
        }
        return SubStr(mLength - count, count);
    }

    /**
     * @brief Alias pour SubStr() (convention Qt-style)
     * 
     * @param pos Position de départ
     * @param count Longueur optionnelle (npos = jusqu'à la fin)
     * @return Sous-vue spécifiée
     */
    NkStringView NkStringBuilder::Mid(SizeType pos, SizeType count) const {
        return SubStr(pos, count);
    }

    /**
     * @brief Crée une copie possédante du contenu actuel
     * 
     * @return Nouvelle instance NkString avec copie des données
     * 
     * @note Allocation mémoire nécessaire : utiliser avec précaution
     *       dans les boucles ou code performance-critical.
     */
    NkString NkStringBuilder::ToString() const {
        return NkString(mBuffer, mLength);
    }

    /**
     * @brief Crée une copie possédante d'une sous-partie
     * 
     * @param pos Position de départ dans le builder
     * @param count Nombre de caractères à copier (npos = jusqu'à la fin)
     * @return Nouvelle instance NkString contenant la sous-partie
     * 
     * @note Équivalent à SubStr(pos, count).ToString().
     */
    NkString NkStringBuilder::ToString(SizeType pos, SizeType count) const {
        return NkString(SubStr(pos, count));
    }

    // =====================================================================
    // SECTION : RECHERCHE ET TESTS DE CONTENU
    // =====================================================================

    /**
     * @brief Recherche la première occurrence d'un caractère
     * 
     * @param ch Caractère à rechercher
     * @param pos Position de départ pour la recherche (défaut: 0)
     * @return Index de la première occurrence, ou npos si non trouvé
     * 
     * @note Parcours linéaire optimisé : O(n) worst-case.
     */
    NkStringBuilder::SizeType NkStringBuilder::Find(
        char ch, 
        SizeType pos) const noexcept {
        
        if (pos >= mLength) {
            return NkStringView::npos;
        }
        for (SizeType i = pos; i < mLength; ++i) {
            if (mBuffer[i] == ch) {
                return i;
            }
        }
        return NkStringView::npos;
    }

    /**
     * @brief Recherche la première occurrence d'une sous-chaîne
     * 
     * @param str Sous-chaîne à rechercher
     * @param pos Position de départ pour la recherche (défaut: 0)
     * @return Index de la première occurrence, ou npos si non trouvé
     * 
     * @note Implémentation naïve O(n*m) : suffisante pour la plupart des cas.
     *       Utilise NkMemCompare pour comparaison efficace.
     */
    NkStringBuilder::SizeType NkStringBuilder::Find(
        NkStringView str, 
        SizeType pos) const noexcept {
        
        if (str.Empty()) {
            return pos;
        }
        if (pos > mLength) {
            return NkStringView::npos;
        }
        if (str.Length() > mLength - pos) {
            return NkStringView::npos;
        }
        for (SizeType i = pos; i <= mLength - str.Length(); ++i) {
            if (memory::NkMemCompare(mBuffer + i, str.Data(), str.Length()) == 0) {
                return i;
            }
        }
        return NkStringView::npos;
    }

    /**
     * @brief Recherche la dernière occurrence d'un caractère
     * 
     * @param ch Caractère à rechercher
     * @param pos Position limite pour recherche arrière (défaut: npos = fin)
     * @return Index de la dernière occurrence, ou npos si non trouvé
     * 
     * @note Recherche depuis min(pos, Length()-1) vers le début.
     */
    NkStringBuilder::SizeType NkStringBuilder::FindLast(
        char ch, 
        SizeType pos) const noexcept {
        
        if (mLength == 0) {
            return NkStringView::npos;
        }
        SizeType searchPos = (pos == NkStringView::npos || pos >= mLength) 
            ? mLength - 1 
            : pos;
        for (SizeType i = searchPos + 1; i > 0; --i) {
            if (mBuffer[i - 1] == ch) {
                return i - 1;
            }
        }
        return NkStringView::npos;
    }

    /**
     * @brief Recherche la dernière occurrence d'une sous-chaîne
     * 
     * @param str Sous-chaîne à rechercher
     * @param pos Position limite pour recherche arrière (défaut: npos)
     * @return Index de la dernière occurrence, ou npos si non trouvé
     */
    NkStringBuilder::SizeType NkStringBuilder::FindLast(
        NkStringView str, 
        SizeType pos) const noexcept {
        
        if (str.Empty()) {
            return (pos < mLength) ? pos : mLength;
        }
        if (str.Length() > mLength) {
            return NkStringView::npos;
        }
        SizeType searchPos = (pos == NkStringView::npos || pos >= mLength - str.Length()) 
            ? mLength - str.Length() 
            : pos;
        for (SizeType i = searchPos + 1; i > 0; --i) {
            SizeType idx = i - 1;
            if (memory::NkMemCompare(mBuffer + idx, str.Data(), str.Length()) == 0) {
                return idx;
            }
        }
        return NkStringView::npos;
    }

    /**
     * @brief Vérifie si le builder contient un caractère donné
     * 
     * @param ch Caractère à rechercher
     * @return true si le caractère est présent au moins une fois
     */
    bool NkStringBuilder::Contains(char ch) const noexcept {
        return Find(ch) != NkStringView::npos;
    }

    /**
     * @brief Vérifie si le builder contient une sous-chaîne donnée
     * 
     * @param str Sous-chaîne à rechercher
     * @return true si str est trouvé dans le builder
     */
    bool NkStringBuilder::Contains(NkStringView str) const noexcept {
        return Find(str) != NkStringView::npos;
    }

    /**
     * @brief Vérifie si le builder commence par un caractère donné
     * 
     * @param ch Caractère à tester en première position
     * @return true si Length() > 0 && Data()[0] == ch
     */
    bool NkStringBuilder::StartsWith(char ch) const noexcept {
        return mLength > 0 && mBuffer[0] == ch;
    }

    /**
     * @brief Vérifie si le builder commence par un préfixe donné
     * 
     * @param prefix Vue représentant le préfixe attendu
     * @return true si les premiers caractères correspondent exactement
     * 
     * @note Retourne false si prefix.Length() > Length().
     */
    bool NkStringBuilder::StartsWith(NkStringView prefix) const noexcept {
        if (prefix.Length() > mLength) {
            return false;
        }
        return memory::NkMemCompare(mBuffer, prefix.Data(), prefix.Length()) == 0;
    }

    /**
     * @brief Vérifie si le builder se termine par un caractère donné
     * 
     * @param ch Caractère à tester en dernière position
     * @return true si Length() > 0 && Data()[Length()-1] == ch
     */
    bool NkStringBuilder::EndsWith(char ch) const noexcept {
        return mLength > 0 && mBuffer[mLength - 1] == ch;
    }

    /**
     * @brief Vérifie si le builder se termine par un suffixe donné
     * 
     * @param suffix Vue représentant le suffixe attendu
     * @return true si les derniers caractères correspondent exactement
     * 
     * @note Retourne false si suffix.Length() > Length().
     */
    bool NkStringBuilder::EndsWith(NkStringView suffix) const noexcept {
        if (suffix.Length() > mLength) {
            return false;
        }
        return memory::NkMemCompare(
            mBuffer + mLength - suffix.Length(), 
            suffix.Data(), 
            suffix.Length()) == 0;
    }

    // =====================================================================
    // SECTION : COMPARAISONS
    // =====================================================================

    /**
     * @brief Compare lexicographiquement avec une NkStringView
     * 
     * @param other Vue de chaîne à comparer
     * @return <0 si this < other, 0 si égal, >0 si this > other
     * 
     * @note Comparaison binaire octet par octet (case-sensitive).
     *       Utilise NkMemCompare pour efficacité sur grands buffers.
     */
    int NkStringBuilder::Compare(NkStringView other) const noexcept {
        SizeType minLen = mLength < other.Length() ? mLength : other.Length();
        if (minLen > 0) {
            int result = memory::NkMemCompare(mBuffer, other.Data(), minLen);
            if (result != 0) {
                return result;
            }
        }
        if (mLength < other.Length()) {
            return -1;
        }
        if (mLength > other.Length()) {
            return 1;
        }
        return 0;
    }

    /**
     * @brief Compare lexicographiquement avec un autre NkStringBuilder
     * 
     * @param other Autre builder à comparer
     * @return <0 si this < other, 0 si égal, >0 si this > other
     * 
     * @note Délègue à Compare(other.View()) pour réutilisation.
     */
    int NkStringBuilder::Compare(const NkStringBuilder& other) const noexcept {
        return Compare(NkStringView(other.mBuffer, other.mLength));
    }

    /**
     * @brief Teste l'égalité binaire avec une NkStringView
     * 
     * @param other Vue de chaîne à comparer
     * @return true si mêmes données et même longueur
     */
    bool NkStringBuilder::Equals(NkStringView other) const noexcept {
        if (mLength != other.Length()) {
            return false;
        }
        return memory::NkMemCompare(mBuffer, other.Data(), mLength) == 0;
    }

    /**
     * @brief Teste l'égalité binaire avec un autre NkStringBuilder
     * 
     * @param other Autre builder à comparer
     * @return true si contenus identiques
     */
    bool NkStringBuilder::Equals(const NkStringBuilder& other) const noexcept {
        if (mLength != other.mLength) {
            return false;
        }
        return memory::NkMemCompare(mBuffer, other.mBuffer, mLength) == 0;
    }

    // =====================================================================
    // SECTION : TRANSFORMATIONS IN-PLACE
    // =====================================================================

    /**
     * @brief Convertit le contenu en minuscules (ASCII) in-place
     * 
     * @return Référence vers this pour chaînage
     * 
     * @note Modifie directement le buffer interne : pas d'allocation supplémentaire.
     *       Conversion ASCII uniquement : pour Unicode, utiliser ICU.
     */
    NkStringBuilder& NkStringBuilder::ToLower() {
        for (SizeType i = 0; i < mLength; ++i) {
            mBuffer[i] = string::NkToLower(mBuffer[i]);
        }
        return *this;
    }

    /**
     * @brief Convertit le contenu en majuscules (ASCII) in-place
     * 
     * @return Référence vers this pour chaînage
     */
    NkStringBuilder& NkStringBuilder::ToUpper() {
        for (SizeType i = 0; i < mLength; ++i) {
            mBuffer[i] = string::NkToUpper(mBuffer[i]);
        }
        return *this;
    }

    /**
     * @brief Met la première lettre en majuscule, le reste en minuscules
     * 
     * @return Référence vers this pour chaînage
     * 
     * @note Ex: "hELLO" -> "Hello"
     *       Conversion ASCII uniquement.
     */
    NkStringBuilder& NkStringBuilder::Capitalize() {
        if (mLength > 0) {
            mBuffer[0] = string::NkToUpper(mBuffer[0]);
            for (SizeType i = 1; i < mLength; ++i) {
                mBuffer[i] = string::NkToLower(mBuffer[i]);
            }
        }
        return *this;
    }

    /**
     * @brief Met chaque mot en Title Case (majuscule après espace)
     * 
     * @return Référence vers this pour chaînage
     * 
     * @note Ex: "hello world" -> "Hello World"
     *       Délimiteurs de mots : espaces, tabulations.
     */
    NkStringBuilder& NkStringBuilder::TitleCase() {
        if (mLength == 0) {
            return *this;
        }
        bool newWord = true;
        for (SizeType i = 0; i < mLength; ++i) {
            if (string::NkIsWhitespace(mBuffer[i]) || mBuffer[i] == '-') {
                newWord = true;
            } else if (newWord) {
                mBuffer[i] = string::NkToUpper(mBuffer[i]);
                newWord = false;
            } else {
                mBuffer[i] = string::NkToLower(mBuffer[i]);
            }
        }
        return *this;
    }

    /**
     * @brief Inverse l'ordre des caractères in-place
     * 
     * @return Référence vers this pour chaînage
     * 
     * @note Ex: "abc" -> "cba"
     *       Complexité O(n/2) : échange symétrique.
     */
    NkStringBuilder& NkStringBuilder::Reverse() {
        for (SizeType i = 0; i < mLength / 2; ++i) {
            char temp = mBuffer[i];
            mBuffer[i] = mBuffer[mLength - i - 1];
            mBuffer[mLength - i - 1] = temp;
        }
        return *this;
    }

    /**
     * @brief Remplit à gauche avec un caractère pour atteindre une largeur cible
     * 
     * @param totalWidth Largeur finale souhaitée
     * @param paddingChar Caractère de remplissage (défaut: espace)
     * @return Référence vers this pour chaînage
     * 
     * @note Si Length() >= totalWidth : no-op.
     *       Ex: sb.PadLeft(5, '0') sur "42" donne "00042"
     */
    NkStringBuilder& NkStringBuilder::PadLeft(
        SizeType totalWidth, 
        char paddingChar) {
        
        if (mLength >= totalWidth) {
            return *this;
        }
        SizeType padCount = totalWidth - mLength;
        Insert(0, padCount, paddingChar);
        return *this;
    }

    /**
     * @brief Remplit à droite avec un caractère pour atteindre une largeur cible
     * 
     * @param totalWidth Largeur finale souhaitée
     * @param paddingChar Caractère de remplissage (défaut: espace)
     * @return Référence vers this pour chaînage
     * 
     * @note Ex: sb.PadRight(5, '.') sur "Hi" donne "Hi..."
     */
    NkStringBuilder& NkStringBuilder::PadRight(
        SizeType totalWidth, 
        char paddingChar) {
        
        if (mLength >= totalWidth) {
            return *this;
        }
        SizeType padCount = totalWidth - mLength;
        Append(padCount, paddingChar);
        return *this;
    }

    /**
     * @brief Centre le contenu avec remplissage symétrique
     * 
     * @param totalWidth Largeur finale souhaitée
     * @param paddingChar Caractère de remplissage (défaut: espace)
     * @return Référence vers this pour chaînage
     * 
     * @note Si padding impair : le caractère excédentaire est ajouté à droite.
     *       Ex: sb.PadCenter(5, '-') sur "X" donne "--X--"
     */
    NkStringBuilder& NkStringBuilder::PadCenter(
        SizeType totalWidth, 
        char paddingChar) {
        
        if (mLength >= totalWidth) {
            return *this;
        }
        SizeType padding = totalWidth - mLength;
        SizeType leftPadding = padding / 2;
        SizeType rightPadding = padding - leftPadding;
        Insert(0, leftPadding, paddingChar);
        Append(rightPadding, paddingChar);
        return *this;
    }

    // =====================================================================
    // SECTION : ITÉRATEURS (COMPATIBILITÉ STL)
    // =====================================================================

    /**
     * @brief Itérateur modifiable vers le premier élément
     * 
     * @return Pointeur modifiable vers mBuffer
     */
    NkStringBuilder::Iterator NkStringBuilder::begin() noexcept {
        return mBuffer;
    }

    /**
     * @brief Itérateur constant vers le premier élément
     * 
     * @return Pointeur constant vers mBuffer
     */
    NkStringBuilder::ConstIterator NkStringBuilder::begin() const noexcept {
        return mBuffer;
    }

    /**
     * @brief Alias const pour begin() (cohérence API STL)
     * 
     * @return Pointeur constant vers mBuffer
     */
    NkStringBuilder::ConstIterator NkStringBuilder::cbegin() const noexcept {
        return mBuffer;
    }

    /**
     * @brief Itérateur modifiable vers après le dernier élément
     * 
     * @return Pointeur modifiable vers mBuffer + Length()
     */
    NkStringBuilder::Iterator NkStringBuilder::end() noexcept {
        return mBuffer + mLength;
    }

    /**
     * @brief Itérateur constant vers après le dernier élément
     * 
     * @return Pointeur constant vers mBuffer + Length()
     */
    NkStringBuilder::ConstIterator NkStringBuilder::end() const noexcept {
        return mBuffer + mLength;
    }

    /**
     * @brief Alias const pour end() (cohérence API STL)
     * 
     * @return Pointeur constant vers mBuffer + Length()
     */
    NkStringBuilder::ConstIterator NkStringBuilder::cend() const noexcept {
        return mBuffer + mLength;
    }

    /**
     * @brief Itérateur inverse modifiable vers le dernier élément
     * 
     * @return Pointeur modifiable vers mBuffer + Length() - 1
     * 
     * @warning Comportement indéfini si Empty() : vérifier avant usage
     */
    NkStringBuilder::ReverseIterator NkStringBuilder::rbegin() noexcept {
        return mBuffer + mLength - 1;
    }

    /**
     * @brief Itérateur inverse constant vers le dernier élément
     * 
     * @return Pointeur constant vers mBuffer + Length() - 1
     */
    NkStringBuilder::ConstReverseIterator NkStringBuilder::rbegin() const noexcept {
        return mBuffer + mLength - 1;
    }

    /**
     * @brief Alias const pour rbegin()
     * 
     * @return Pointeur constant vers mBuffer + Length() - 1
     */
    NkStringBuilder::ConstReverseIterator NkStringBuilder::crbegin() const noexcept {
        return mBuffer + mLength - 1;
    }

    /**
     * @brief Itérateur inverse modifiable vers avant le premier élément
     * 
     * @return Pointeur modifiable vers mBuffer - 1
     * 
     * @note Position sentinelle : ne pas déréférencer
     */
    NkStringBuilder::ReverseIterator NkStringBuilder::rend() noexcept {
        return mBuffer - 1;
    }

    /**
     * @brief Itérateur inverse constant vers avant le premier élément
     * 
     * @return Pointeur constant vers mBuffer - 1
     */
    NkStringBuilder::ConstReverseIterator NkStringBuilder::rend() const noexcept {
        return mBuffer - 1;
    }

    /**
     * @brief Alias const pour rend()
     * 
     * @return Pointeur constant vers mBuffer - 1
     */
    NkStringBuilder::ConstReverseIterator NkStringBuilder::crend() const noexcept {
        return mBuffer - 1;
    }

    // =====================================================================
    // SECTION : OPÉRATIONS STYLE FLUX (STREAM-LIKE)
    // =====================================================================

    /**
     * @brief Écrit des données brutes dans le builder
     * 
     * @param data Pointeur vers les données à écrire
     * @param size Nombre de bytes à copier
     * @return Référence vers this pour chaînage
     * 
     * @note Permet d'ajouter des données binaires ou non-textuelles.
     *       Aucune interprétation de encoding : copie byte-à-byte.
     */
    NkStringBuilder& NkStringBuilder::Write(const void* data, SizeType size) {
        return Append(static_cast<const char*>(data), size);
    }

    /**
     * @brief Écrit un caractère unique
     * 
     * @param ch Caractère à écrire
     * @return Référence vers this pour chaînage
     * 
     * @note Alias pour Append(ch). Nom alternatif pour style flux.
     */
    NkStringBuilder& NkStringBuilder::WriteChar(char ch) {
        return Append(ch);
    }

    /**
     * @brief Écrit une chaîne suivie d'un saut de ligne
     * 
     * @param str Vue de chaîne à écrire avant le newline (optionnel)
     * @return Référence vers this pour chaînage
     * 
     * @note Alias pour AppendLine(str).
     *       Si str est vide : ajoute uniquement le newline.
     */
    NkStringBuilder& NkStringBuilder::WriteLine(NkStringView str) {
        return AppendLine(str);
    }

    // =====================================================================
    // SECTION : OPÉRATEURS DE CONVERSION
    // =====================================================================

    /**
     * @brief Conversion explicite vers NkString
     * 
     * @return Nouvelle instance NkString avec copie des données
     * 
     * @note Allocation mémoire nécessaire.
     *       Syntaxe : NkString s = static_cast<NkString>(sb);
     */
    NkStringBuilder::operator NkString() const {
        return ToString();
    }

    /**
     * @brief Conversion explicite vers NkStringView
     * 
     * @return Vue en lecture seule sur les données internes
     * 
     * @note Aucune copie : la vue référence le buffer interne.
     *       @warning La vue devient invalide si le builder est modifié.
     */
    NkStringBuilder::operator NkStringView() const noexcept {
        return NkStringView(mBuffer, mLength);
    }

    /**
     * @brief Conversion implicite vers C-string
     * 
     * @return Pointeur constant vers les données null-terminated
     * 
     * @note Permet d'utiliser le builder directement dans des APIs C.
     *       Ex: printf("%s", static_cast<const char*>(sb));
     *       @warning Le pointeur devient invalide après modification du builder.
     */
    NkStringBuilder::operator const char*() const noexcept {
        return CStr();
    }

    // =====================================================================
    // SECTION : UTILITAIRES DIVERS
    // =====================================================================

    /**
     * @brief Échange le contenu avec un autre builder en O(1)
     * 
     * @param other Référence vers l'autre NkStringBuilder à échanger
     * 
     * @note Échange uniquement les pointeurs et métadonnées.
     *       Aucune copie de données : très efficace.
     *       Noexcept : garantie pour conteneurs STL-like.
     *       Utilise traits::NkSwap pour échange générique.
     */
    void NkStringBuilder::Swap(NkStringBuilder& other) noexcept {
        (traits::NkSwap)(mAllocator, other.mAllocator);
        (traits::NkSwap)(mBuffer, other.mBuffer);
        (traits::NkSwap)(mLength, other.mLength);
        (traits::NkSwap)(mCapacity, other.mCapacity);
    }

    /**
     * @brief Calcule un hash rapide du contenu actuel
     * 
     * @return Valeur de hash SizeType
     * 
     * @note Algorithme FNV-1a 64 bits : bon compromis distribution/performance.
     *       Non-cryptographique : pour tables de hachage uniquement.
     */
    NkStringBuilder::SizeType NkStringBuilder::Hash() const noexcept {
        return string::NkHashFNV1a64(NkStringView(mBuffer, mLength));
    }

    // =====================================================================
    // SECTION : MÉTHODES STATIQUES - JOIN DE COLLECTIONS
    // =====================================================================

    /**
     * @brief Joint un tableau de NkStringView avec un séparateur
     * 
     * @param separator Vue du séparateur à insérer entre les éléments
     * @param strings Pointeur vers le premier élément du tableau de vues
     * @param count Nombre d'éléments dans le tableau
     * @return Nouveau NkStringBuilder contenant la concaténation
     * 
     * @note Pré-alloue la taille exacte pour éviter les réallocations multiples.
     *       Gère correctement le cas count == 0 (retourne builder vide).
     */
    NkStringBuilder NkStringBuilder::Join(
        NkStringView separator, 
        const NkStringView* strings, 
        SizeType count) {
        
        NkStringBuilder result;
        if (count == 0) {
            return result;
        }
        result.Append(strings[0]);
        for (SizeType i = 1; i < count; ++i) {
            result.Append(separator);
            result.Append(strings[i]);
        }
        return result;
    }

    /**
     * @brief Joint un tableau de NkStringBuilder avec un séparateur
     * 
     * @param separator Vue du séparateur à insérer entre les éléments
     * @param builders Pointeur vers le premier élément du tableau de builders
     * @param count Nombre d'éléments dans le tableau
     * @return Nouveau NkStringBuilder contenant la concaténation
     * 
     * @note Utilise View() de chaque builder pour éviter copies intermédiaires.
     */
    NkStringBuilder NkStringBuilder::Join(
        NkStringView separator, 
        const NkStringBuilder* builders, 
        SizeType count) {
        
        NkStringBuilder result;
        if (count == 0) {
            return result;
        }
        result.Append(builders[0]);
        for (SizeType i = 1; i < count; ++i) {
            result.Append(separator);
            result.Append(builders[i]);
        }
        return result;
    }

} // namespace nkentseu

// -----------------------------------------------------------------------------
// EXEMPLES D'UTILISATION
// -----------------------------------------------------------------------------
/*
    // =====================================================================
    // Exemple 1 : Construction fluide avec opérateur <<
    // =====================================================================
    {
        nkentseu::NkStringBuilder sb;
        sb << "User: " << "Alice"
           << ", Age: " << 30
           << ", Active: " << true
           << ", Score: " << 98.5f
           << "\n";
        
        nkentseu::NkString result = sb.ToString();
        // "User: Alice, Age: 30, Active: true, Score: 98.500000\n"
    }

    // =====================================================================
    // Exemple 2 : Concaténation efficace en boucle (évite N allocations)
    // =====================================================================
    {
        // ❌ À ÉVITER : NkString en boucle -> N réallocations
        // nkentseu::NkString bad;
        // for (int i = 0; i < 1000; ++i) {
        //     bad += "chunk";  // Réallocation à chaque itération après SSO
        // }
        
        // ✅ PRÉFÉRER : NkStringBuilder avec buffer croissant
        nkentseu::NkStringBuilder sb;
        sb.Reserve(5000);  // Pré-allocation optionnelle si taille estimée connue
        for (int i = 0; i < 1000; ++i) {
            sb.Append("chunk");
        }
        nkentseu::NkString result = sb.ToString();  // Une seule allocation finale
    }

    // =====================================================================
    // Exemple 3 : Formatage de logs structurés
    // =====================================================================
    {
        nkentseu::NkStringBuilder BuildLogEntry(
            const char* level,
            uint64_t timestamp,
            nkentseu::NkStringView message) {
            
            return nkentseu::NkStringBuilder()
                .Append('[')
                .Append(level)
                .Append("] @")
                .AppendHex(timestamp, true)  // "0x..." prefix
                .Append(" | ")
                .Append(message.Trimmed())
                .AppendLine()
                .ToString();
        }
        
        // Usage :
        // BuildLogEntry("ERROR", 0x1234567890ABCDEF, "  Connection failed  ")
        // -> "[ERROR] @0x1234567890abcdef | Connection failed\n"
    }

    // =====================================================================
    // Exemple 4 : Génération de CSV avec Join statique
    // =====================================================================
    {
        std::vector<nkentseu::NkStringView> fields = {"Name", "Age", "City"};
        auto csvLine = nkentseu::NkStringBuilder::Join(",", fields).ToString();
        // "Name,Age,City"
        
        // Avec données :
        std::vector<nkentseu::NkString> row = {"Alice", "30", "Paris"};
        auto csvRow = nkentseu::NkStringBuilder::Join(",", row).ToString();
        // "Alice,30,Paris"
    }

    // =====================================================================
    // Exemple 5 : Transformation in-place avec chaînage
    // =====================================================================
    {
        nkentseu::NkStringBuilder sb("  hello   world  ");
        sb.Trim()                    // "hello   world"
          .ReplaceAll(' ', '_')      // "hello___world"
          .ToUpper()                 // "HELLO___WORLD"
          .PadCenter(20, '-');       // "--HELLO___WORLD---"
        
        nkentseu::NkString result = sb.ToString();
    }

    // =====================================================================
    // Exemple 6 : Insertion et remplacement précis
    // =====================================================================
    {
        nkentseu::NkStringBuilder sb("HelloWorld");
        
        // Insérer un espace au milieu
        sb.Insert(5, ' ');  // "Hello World"
        
        // Remplacer "World" par "nkentseu"
        usize pos = sb.Find("World");
        if (pos != nkentseu::NkStringView::npos) {
            sb.Replace(pos, 5, "nkentseu");  // "Hello nkentseu"
        }
        
        // Supprimer les 6 premiers caractères
        sb.RemovePrefix(6);  // "nkentseu"
    }

    // =====================================================================
    // Exemple 7 : Conversion numérique avec différentes bases
    // =====================================================================
    {
        nkentseu::NkStringBuilder sb;
        
        uint32 value = 255;
        
        sb.Append("Decimal: ")
          .Append(value)           // "255"
          .Append(", Hex: ")
          .AppendHex(value, true)  // "0xff"
          .Append(", Binary: ")
          .AppendBinary(value, 8)  // "11111111"
          .Append(", Octal: ")
          .AppendOctal(value, true);  // "0377"
        
        // Résultat : "Decimal: 255, Hex: 0xff, Binary: 11111111, Octal: 0377"
    }

    // =====================================================================
    // Exemple 8 : Gestion d'allocateur personnalisé pour pooling
    // =====================================================================
    {
        // Supposons un allocateur pool pour builders temporaires
        class TempAllocator : public nkentseu::memory::NkIAllocator {
            // ... implémentation spécifique ...
        };
        
        TempAllocator tempAlloc;
        
        // Création de builders avec allocateur pool
        nkentseu::NkStringBuilder sb1(256, tempAlloc);
        nkentseu::NkStringBuilder sb2(256, tempAlloc);
        
        sb1 << "Temp data 1";
        sb2 << "Temp data 2";
        
        // Utilisation...
        ProcessData(sb1.CStr(), sb2.CStr());
        
        // Libération groupée possible si l'allocateur supporte le reset
        // tempAlloc.Reset();  // Libère tout le pool en O(1)
    }

    // =====================================================================
    // Exemple 9 : Vue temporaire avec précaution de durée de vie
    // =====================================================================
    {
        nkentseu::NkStringBuilder sb("Hello World");
        
        // ✅ BON : Utiliser la vue immédiatement, avant modification du builder
        nkentseu::NkStringView view = sb.View();
        ProcessView(view);  // Traitement immédiat : vue valide
        
        // ❌ DANGEREUX : Stocker la vue puis modifier le builder
        // auto dangerous = sb.View();
        // sb.Append(" more");  // ⚠️ La vue 'dangerous' peut être invalide maintenant !
        
        // ✅ BON : Si besoin de conserver après modification, convertir en NkString
        nkentseu::NkString owned = sb.ToString();  // Copie possédante
        sb.Append(" more");  // Safe : owned reste valide
    }

    // =====================================================================
    // Exemple 10 : Bonnes pratiques de performance
    // =====================================================================
    {
        // ✅ PRÉFÉRER : Pré-allouer si taille finale estimable
        nkentseu::NkStringBuilder sb;
        sb.Reserve(1024);  // Évite ~3-4 réallocations pour 1KB de données
        
        // ✅ PRÉFÉRER : Utiliser Append(str, length) si longueur connue
        const char* data = GetLargeBuffer();
        usize len = GetLargeBufferSize();
        sb.Append(data, len);  // Évite le calcul de strlen
        
        // ✅ PRÉFÉRER : Chaîner les appels pour éviter les temporaires
        sb.Append("Prefix: ")
          .Append(value)
          .Append(" suffix");  // Un seul builder, pas de temporaires intermédiaires
        
        // ✅ PRÉFÉRER : Utiliser ToString() une seule fois à la fin
        nkentseu::NkString final = sb.ToString();  // Allocation unique finale
        // Éviter : sb.ToString() dans une boucle -> N allocations inutiles
    }
*/