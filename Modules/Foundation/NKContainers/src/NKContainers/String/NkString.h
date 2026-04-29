// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\NkString.h
// DESCRIPTION: Classe de chaîne dynamique avec Small String Optimization (SSO)
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_STRING_NKSTRING_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_STRING_NKSTRING_H_INCLUDED

    // -------------------------------------------------------------------------
    // Inclusions standard
    // -------------------------------------------------------------------------
    #include <cstdarg>

    // -------------------------------------------------------------------------
    // Inclusions locales du projet
    // -------------------------------------------------------------------------
    #include "NkStringView.h"
    #include "NKMemory/NkAllocator.h"
    #include "NKCore/NkConfig.h"  // Pour NK_STRING_SSO_SIZE

    // -------------------------------------------------------------------------
    // Namespace principal
    // -------------------------------------------------------------------------
    namespace nkentseu {

        // =====================================================================
        // CLASSE : NkString
        // =====================================================================

        /**
         * @brief Classe de chaîne dynamique avec optimisation SSO
         * 
         * Cette classe fournit une implémentation de chaîne de caractères
         * performante et flexible, inspirée de std::string mais sans dépendance
         * à la STL, avec les caractéristiques suivantes :
         * 
         * 🔹 Small String Optimization (SSO) :
         *    - Chaînes courtes (<= NK_STRING_SSO_SIZE) stockées inline
         *    - Aucune allocation heap pour les cas fréquents
         *    - Réduction significative des allocations mémoire
         * 
         * 🔹 Allocation dynamique pour grandes chaînes :
         *    - Croissance automatique avec stratégie de sur-allocation
         *    - Support des allocateurs personnalisés via NkIAllocator
         *    - Gestion explicite de la mémoire pour contrôle fin
         * 
         * 🔹 Compatibilité C :
         *    - Données toujours null-terminated
         *    - Méthode CStr() pour interop avec APIs C
         *    - Conversion implicite vers NkStringView
         * 
         * 🔹 Interface riche :
         *    - Opérateurs de concaténation, comparaison, accès
         *    - Méthodes de recherche, transformation, conversion
         *    - Formatage style printf et moderne {placeholder}
         * 
         * @note Thread-safety : Cette classe n'est PAS thread-safe par défaut.
         *       La synchronisation externe est requise pour un accès concurrent.
         * 
         * @warning Gestion de la mémoire : Lors de l'utilisation d'allocateurs
         *          personnalisés, garantir que le même allocateur est utilisé
         *          pour l'allocation et la libération d'une instance donnée.
         */
        class NKENTSEU_CORE_API NkString {

            // =================================================================
            // TYPES PUBLICS ET CONSTANTES
            // =================================================================
            public:

                /// Type des éléments stockés (caractères)
                using ValueType = char;

                /// Type utilisé pour les tailles, indices et capacités
                using SizeType = usize;

                /// Valeur spéciale représentant "non trouvé" ou position invalide
                static constexpr SizeType npos = static_cast<SizeType>(-1);

                /// Taille du buffer SSO : chaînes <= cette valeur stockées inline
                static constexpr SizeType SSO_SIZE = NK_STRING_SSO_SIZE;

            // =================================================================
            // CONSTRUCTEURS
            // =================================================================
            public:

                /**
                 * @brief Constructeur par défaut : chaîne vide
                 * 
                 * Initialise une NkString vide avec l'allocateur par défaut.
                 * Aucune allocation mémoire n'est effectuée (SSO utilisé).
                 * 
                 * @note Complexité : O(1), constante
                 */
                NkString();

                /**
                 * @brief Constructeur avec allocateur personnalisé
                 * 
                 * @param allocator Référence vers l'allocateur à utiliser
                 * 
                 * @note Toutes les allocations futures utiliseront cet allocateur.
                 *       L'allocateur doit rester valide pendant toute la durée
                 *       de vie de l'instance NkString.
                 */
                explicit NkString(memory::NkIAllocator& allocator);

                /**
                 * @brief Constructeur depuis une chaîne C terminée par null
                 * 
                 * @param str Pointeur vers une chaîne C-style (const char*)
                 * 
                 * @note Copie les données de str dans cette instance.
                 *       Si str est nullptr, initialise une chaîne vide.
                 *       Longueur calculée automatiquement via strlen-like.
                 */
                NkString(const char* str);

                /**
                 * @brief Constructeur depuis C-string avec allocateur personnalisé
                 * 
                 * @param str Pointeur vers chaîne C terminée par null
                 * @param allocator Référence vers l'allocateur à utiliser
                 * 
                 * @note Identique au constructeur sans allocateur, mais permet
                 *       de spécifier explicitement la stratégie d'allocation.
                 */
                NkString(const char* str, memory::NkIAllocator& allocator);

                /**
                 * @brief Constructeur depuis buffer + longueur explicite
                 * 
                 * @param str Pointeur vers les données caractères
                 * @param length Nombre de caractères à copier depuis str
                 * 
                 * @note Permet de copier des sous-parties de buffer ou des données
                 *       non-terminées par null. Ne copie PAS le null terminator
                 *       automatiquement : il est ajouté internement.
                 */
                NkString(const char* str, SizeType length);

                /**
                 * @brief Constructeur depuis buffer + longueur avec allocateur
                 * 
                 * @param str Pointeur vers les données caractères
                 * @param length Nombre de caractères à copier
                 * @param allocator Référence vers l'allocateur à utiliser
                 */
                NkString(const char* str, SizeType length, memory::NkIAllocator& allocator);

                /**
                 * @brief Constructeur depuis NkStringView
                 * 
                 * @param view Vue de chaîne à copier dans cette instance
                 * 
                 * @note Crée une copie possédante des données référencées par view.
                 *       La vue source peut être détruite après construction.
                 */
                NkString(NkStringView view);

                /**
                 * @brief Constructeur depuis NkStringView avec allocateur
                 * 
                 * @param view Vue de chaîne à copier
                 * @param allocator Référence vers l'allocateur à utiliser
                 */
                NkString(NkStringView view, memory::NkIAllocator& allocator);

                /**
                 * @brief Constructeur fill : répète un caractère N fois
                 * 
                 * @param count Nombre de répétitions du caractère
                 * @param ch Caractère à répéter dans la chaîne résultante
                 * 
                 * @note Ex: NkString(5, 'x') crée "xxxxx"
                 *       Utile pour padding, masquage, initialisation.
                 */
                NkString(SizeType count, char ch);

                /**
                 * @brief Constructeur fill avec allocateur personnalisé
                 * 
                 * @param count Nombre de répétitions
                 * @param ch Caractère à répéter
                 * @param allocator Référence vers l'allocateur à utiliser
                 */
                NkString(SizeType count, char ch, memory::NkIAllocator& allocator);

                /**
                 * @brief Constructeur de copie
                 * 
                 * @param other Instance NkString à copier
                 * 
                 * @note Effectue une copie profonde des données.
                 *       Utilise l'allocateur par défaut ou celui de other
                 *       selon l'implémentation.
                 */
                NkString(const NkString& other);

                /**
                 * @brief Constructeur de copie avec allocateur personnalisé
                 * 
                 * @param other Instance NkString à copier
                 * @param allocator Allocateur pour la nouvelle instance
                 * 
                 * @note Permet de copier une chaîne tout en changeant
                 *       sa stratégie d'allocation mémoire.
                 */
                NkString(const NkString& other, memory::NkIAllocator& allocator);

                /**
                 * @brief Constructeur de déplacement
                 * 
                 * @param other Instance NkString à déplacer
                 * 
                 * @note Transfère la propriété des ressources de other vers this.
                 *       other devient une chaîne vide après l'opération.
                 *       Complexité : O(1), aucune copie de données.
                 */
                NkString(NkString&& other) noexcept;

                /**
                 * @brief Destructeur
                 * 
                 * @note Libère les ressources heap si nécessaire.
                 *       Aucune action si SSO était utilisé.
                 *       Garantit aucune fuite mémoire.
                 */
                ~NkString();

            // =================================================================
            // OPÉRATEURS D'AFFECTATION
            // =================================================================
            public:

                /**
                 * @brief Affectation par copie
                 * 
                 * @param other Instance source à copier
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Gère l'auto-affectation. Réalloue si capacité insuffisante.
                 *       Préserve l'allocateur courant de this.
                 */
                NkString& operator=(const NkString& other);

                /**
                 * @brief Affectation par déplacement
                 * 
                 * @param other Instance source à déplacer
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Transfère les ressources de other vers this.
                 *       other devient vide après l'opération.
                 *       Noexcept : garantit pas d'exception pour sécurité STL.
                 */
                NkString& operator=(NkString&& other) noexcept;

                /**
                 * @brief Affectation depuis une chaîne C-style
                 * 
                 * @param str Chaîne C terminée par null à copier
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Remplace le contenu actuel par une copie de str.
                 *       Gère le cas str == nullptr (devient chaîne vide).
                 */
                NkString& operator=(const char* str);

                /**
                 * @brief Affectation depuis NkStringView
                 * 
                 * @param view Vue de chaîne à copier
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Copie les données référencées par view.
                 *       Plus efficace que conversion view -> NkString -> assign.
                 */
                NkString& operator=(NkStringView view);

                /**
                 * @brief Affectation depuis un caractère unique
                 * 
                 * @param ch Caractère à assigner comme contenu unique
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Équivalent à Assign(1, ch).
                 *       Utile pour réinitialisation rapide à un caractère.
                 */
                NkString& operator=(char ch);

            // =================================================================
            // ACCÈS AUX ÉLÉMENTS
            // =================================================================
            public:

                /**
                 * @brief Accès indexé sans vérification de bornes
                 * 
                 * @param index Position du caractère à accéder
                 * @return Référence modifiable vers le caractère
                 * 
                 * @warning Comportement indéfini si index >= Length().
                 *          Utiliser At() en mode développement pour sécurité.
                 */
                char& operator[](SizeType index);

                /**
                 * @brief Accès indexé constant sans vérification de bornes
                 * 
                 * @param index Position du caractère à accéder
                 * @return Référence constante vers le caractère
                 */
                const char& operator[](SizeType index) const;

                /**
                 * @brief Accès indexé avec vérification de bornes
                 * 
                 * @param index Position du caractère à accéder
                 * @return Référence modifiable vers le caractère
                 * 
                 * @note Lève une assertion en mode debug si index hors bornes.
                 *       Préférer cette méthode pour code robuste.
                 */
                char& At(SizeType index);

                /**
                 * @brief Accès indexé constant avec vérification de bornes
                 * 
                 * @param index Position du caractère à accéder
                 * @return Référence constante vers le caractère
                 */
                const char& At(SizeType index) const;

                /**
                 * @brief Accès au premier caractère (modifiable)
                 * 
                 * @return Référence modifiable vers le caractère en position 0
                 * 
                 * @warning Assertion si Empty() : vérifier avant appel.
                 */
                char& Front();

                /**
                 * @brief Accès au premier caractère (lecture seule)
                 * 
                 * @return Référence constante vers le caractère en position 0
                 */
                const char& Front() const;

                /**
                 * @brief Accès au dernier caractère (modifiable)
                 * 
                 * @return Référence modifiable vers le dernier caractère
                 * 
                 * @warning Assertion si Empty() : vérifier avant appel.
                 */
                char& Back();

                /**
                 * @brief Accès au dernier caractère (lecture seule)
                 * 
                 * @return Référence constante vers le dernier caractère
                 */
                const char& Back() const;

                /**
                 * @brief Accès aux données brutes (version constante)
                 * 
                 * @return Pointeur constant vers le premier caractère
                 * 
                 * @note Retourne toujours un pointeur valide, même si Empty().
                 *       Les données sont garanties null-terminated.
                 */
                const char* Data() const noexcept;

                /**
                 * @brief Accès aux données brutes (version modifiable)
                 * 
                 * @return Pointeur modifiable vers le premier caractère
                 * 
                 * @warning Modifier les données via ce pointeur peut invalider
                 *          les métadonnées internes (longueur, null-terminator).
                 *          Utiliser avec précaution, préférer les méthodes API.
                 */
                char* Data() noexcept;

                /**
                 * @brief Accès compatible C-string (null-terminated garanti)
                 * 
                 * @return Pointeur constant vers chaîne C-style
                 * 
                 * @note Méthode privilégiée pour interop avec APIs C.
                 *       Le pointeur reste valide jusqu'à la prochaine
                 *       modification non-const de cette instance.
                 */
                const char* CStr() const noexcept;

            // =================================================================
            // CAPACITÉ ET ÉTAT
            // =================================================================
            public:

                /**
                 * @brief Retourne le nombre de caractères dans la chaîne
                 * 
                 * @return Longueur actuelle en caractères (exclut null-terminator)
                 * 
                 * @note Complexité O(1) : valeur pré-calculée et maintenue.
                 *       Ne pas confondre avec Capacity() qui inclut l'espace alloué.
                 */
                SizeType Length() const noexcept;

                /**
                 * @brief Alias pour Length() (convention STL)
                 * 
                 * @return Nombre d'éléments dans la chaîne
                 */
                SizeType Size() const noexcept;

                /**
                 * @brief Retourne la capacité totale allouée
                 * 
                 * @return Nombre maximal de caractères stockables sans réallocation
                 * 
                 * @note Capacity() >= Length() toujours.
                 *       La différence représente l'espace de croissance réservé.
                 */
                SizeType Capacity() const noexcept;

                /**
                 * @brief Teste si la chaîne est vide
                 * 
                 * @return true si Length() == 0, false sinon
                 * 
                 * @note Plus efficace que Length() == 0 dans certains contextes.
                 *       Ne dit rien sur la capacité allouée.
                 */
                bool Empty() const noexcept;

                /**
                 * @brief Réserve de la capacité pour croissance future
                 * 
                 * @param newCapacity Nouvelle capacité minimale souhaitée
                 * 
                 * @note Si newCapacity <= Capacity(), aucune action.
                 *       Si newCapacity > Capacity(), alloue et copie les données.
                 *       Utile pour éviter les réallocations multiples en boucle.
                 */
                void Reserve(SizeType newCapacity);

                /**
                 * @brief Réduit la capacité au strict nécessaire
                 * 
                 * @note Libère l'espace excédentaire si Capacity() > Length().
                 *       Peut déclencher une réallocation et copie si heap utilisé.
                 *       Aucune action si SSO est actif.
                 */
                void ShrinkToFit();

                /**
                 * @brief Vide la chaîne sans libérer la capacité
                 * 
                 * @note Met Length() à 0, préserve Capacity() pour réutilisation.
                 *       Aucune libération mémoire : efficace pour réutilisation.
                 *       Le null-terminator est maintenu en position 0.
                 */
                void Clear() noexcept;

            // =================================================================
            // MODIFICATEURS : AJOUT ET CONCATÉNATION
            // =================================================================
            public:

                /**
                 * @brief Ajoute une chaîne C à la fin
                 * 
                 * @param str Chaîne C terminée par null à ajouter
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Réalloue si nécessaire pour accueillir les nouvelles données.
                 *       Gère str == nullptr comme chaîne vide (no-op).
                 */
                NkString& Append(const char* str);

                /**
                 * @brief Ajoute N caractères depuis un buffer à la fin
                 * 
                 * @param str Pointeur vers les données à ajouter
                 * @param length Nombre de caractères à copier depuis str
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Permet d'ajouter des sous-parties ou données non-terminées.
                 *       Plus efficace que Append(str) si length < strlen(str).
                 */
                NkString& Append(const char* str, SizeType length);

                /**
                 * @brief Ajoute le contenu d'une autre NkString à la fin
                 * 
                 * @param str Instance NkString à concaténer
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Optimisé : évite conversion intermédiaire.
                 *       Gère l'auto-concaténation correctement.
                 */
                NkString& Append(const NkString& str);

                /**
                 * @brief Ajoute le contenu d'une NkStringView à la fin
                 * 
                 * @param view Vue de chaîne à concaténer
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Efficace : copie directe depuis la vue sans allocation temporaire.
                 */
                NkString& Append(NkStringView view);

                /**
                 * @brief Ajoute N répétitions d'un caractère à la fin
                 * 
                 * @param count Nombre de fois répéter le caractère
                 * @param ch Caractère à répéter
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Ex: str.Append(3, '-') ajoute "---" à la fin.
                 *       Utile pour padding, séparateurs, masquage.
                 */
                NkString& Append(SizeType count, char ch);

                /**
                 * @brief Ajoute un caractère unique à la fin
                 * 
                 * @param ch Caractère à ajouter
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Équivalent à Append(1, ch).
                 *       Méthode la plus courante pour construction caractère par caractère.
                 */
                NkString& Append(char ch);

            // =================================================================
            // OPÉRATEURS DE CONCATÉNATION (+=)
            // =================================================================
            public:

                /**
                 * @brief Opérateur += pour chaîne C
                 * 
                 * @param str Chaîne C à concaténer
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Alias pour Append(str). Syntaxe plus concise.
                 */
                NkString& operator+=(const char* str);

                /**
                 * @brief Opérateur += pour NkString
                 * 
                 * @param str Instance NkString à concaténer
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Alias pour Append(str). Gère l'auto-référence.
                 */
                NkString& operator+=(const NkString& str);

                /**
                 * @brief Opérateur += pour NkStringView
                 * 
                 * @param view Vue de chaîne à concaténer
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Alias pour Append(view). Efficace sans copie temporaire.
                 */
                NkString& operator+=(NkStringView view);

                /**
                 * @brief Opérateur += pour caractère unique
                 * 
                 * @param ch Caractère à ajouter à la fin
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Alias pour Append(ch). Syntaxe intuitive.
                 */
                NkString& operator+=(char ch);

            // =================================================================
            // MODIFICATEURS : INSERTION ET SUPPRESSION
            // =================================================================
            public:

                /**
                 * @brief Insère une chaîne C à une position donnée
                 * 
                 * @param pos Index où insérer les nouvelles données (0 = début)
                 * @param str Chaîne C à insérer
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Décale les caractères existants vers la droite.
                 *       Assertion si pos > Length().
                 */
                NkString& Insert(SizeType pos, const char* str);

                /**
                 * @brief Insère une NkString à une position donnée
                 * 
                 * @param pos Index d'insertion
                 * @param str Instance NkString à insérer
                 * @return Référence vers this pour chaînage
                 */
                NkString& Insert(SizeType pos, const NkString& str);

                /**
                 * @brief Insère une NkStringView à une position donnée
                 * 
                 * @param pos Index d'insertion
                 * @param view Vue de chaîne à insérer
                 * @return Référence vers this pour chaînage
                 */
                NkString& Insert(SizeType pos, NkStringView view);

                /**
                 * @brief Insère N répétitions d'un caractère à une position
                 * 
                 * @param pos Index d'insertion
                 * @param count Nombre de répétitions du caractère
                 * @param ch Caractère à insérer
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Ex: str.Insert(2, 3, '*') sur "abcd" donne "ab***cd"
                 */
                NkString& Insert(SizeType pos, SizeType count, char ch);

                /**
                 * @brief Supprime une plage de caractères
                 * 
                 * @param pos Index de début de la plage à supprimer (défaut: 0)
                 * @param count Nombre de caractères à supprimer (défaut: npos = jusqu'à la fin)
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Décale les caractères suivants vers la gauche.
                 *       Ne réduit pas automatiquement la capacité : utiliser ShrinkToFit() si nécessaire.
                 */
                NkString& Erase(SizeType pos = 0, SizeType count = npos);

                /**
                 * @brief Ajoute un caractère à la fin (alias Push)
                 * 
                 * @param ch Caractère à ajouter
                 * 
                 * @note Équivalent à Append(ch). Nom convention STL.
                 */
                void PushBack(char ch);

                /**
                 * @brief Supprime le dernier caractère
                 * 
                 * @note Assertion si Empty().
                 *       Ne réduit pas la capacité : efficient pour pop/push répétés.
                 */
                void PopBack();

            // =================================================================
            // MODIFICATEURS : REMPLACEMENT ET REDIMENSIONNEMENT
            // =================================================================
            public:

                /**
                 * @brief Remplace une plage par une chaîne C
                 * 
                 * @param pos Index de début de la plage à remplacer
                 * @param count Nombre de caractères à remplacer
                 * @param str Chaîne C de remplacement
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Équivalent à Erase(pos, count) + Insert(pos, str).
                 *       Plus efficace car optimisé en une seule passe.
                 */
                NkString& Replace(SizeType pos, SizeType count, const char* str);

                /**
                 * @brief Remplace une plage par une NkString
                 * 
                 * @param pos Index de début de la plage
                 * @param count Nombre de caractères à remplacer
                 * @param str Instance NkString de remplacement
                 * @return Référence vers this pour chaînage
                 */
                NkString& Replace(SizeType pos, SizeType count, const NkString& str);

                /**
                 * @brief Remplace une plage par une NkStringView
                 * 
                 * @param pos Index de début de la plage
                 * @param count Nombre de caractères à remplacer
                 * @param view Vue de chaîne de remplacement
                 * @return Référence vers this pour chaînage
                 */
                NkString& Replace(SizeType pos, SizeType count, NkStringView view);

                /**
                 * @brief Redimensionne la chaîne à une longueur cible
                 * 
                 * @param count Nouvelle longueur souhaitée
                 * 
                 * @note Si count < Length() : tronque à count caractères.
                 *       Si count > Length() : étend avec caractères null ('\\0').
                 *       Peut déclencher réallocation si count > Capacity().
                 */
                void Resize(SizeType count);

                /**
                 * @brief Redimensionne avec caractère de remplissage personnalisé
                 * 
                 * @param count Nouvelle longueur souhaitée
                 * @param ch Caractère à utiliser pour l'extension
                 * 
                 * @note Si count > Length() : remplit avec ch au lieu de '\\0'.
                 *       Utile pour padding avec espace, zéro, astérisque, etc.
                 */
                void Resize(SizeType count, char ch);

                /**
                 * @brief Échange le contenu avec une autre instance en O(1)
                 * 
                 * @param other Référence vers l'autre NkString à échanger
                 * 
                 * @note Échange uniquement les pointeurs et métadonnées.
                 *       Aucune copie de données : très efficace.
                 *       Noexcept : garantie pour conteneurs STL-like.
                 */
                void Swap(NkString& other) noexcept;

            // =================================================================
            // OPÉRATIONS DE SOUS-CHAÎNES ET COMPARAISON
            // =================================================================
            public:

                /**
                 * @brief Extrait une sous-chaîne possédante
                 * 
                 * @param pos Position de départ dans la chaîne courante
                 * @param count Nombre maximal de caractères à extraire (npos = jusqu'à la fin)
                 * @return Nouvelle instance NkString contenant la sous-partie
                 * 
                 * @note Alloue une nouvelle chaîne : utiliser NkStringView::SubStr()
                 *       pour une vue non-possessive si la copie n'est pas nécessaire.
                 *       Assertion si pos > Length().
                 */
                NkString SubStr(SizeType pos = 0, SizeType count = npos) const;

                /**
                 * @brief Compare lexicographiquement avec une autre NkString
                 * 
                 * @param other Instance à comparer
                 * @return <0 si this < other, 0 si égal, >0 si this > other
                 * 
                 * @note Comparaison binaire octet par octet (case-sensitive).
                 *       Utilise memcmp-like pour efficacité sur grands buffers.
                 */
                int32 Compare(const NkString& other) const noexcept;

                /**
                 * @brief Compare avec une NkStringView
                 * 
                 * @param other Vue à comparer
                 * @return Résultat de comparaison lexicographique
                 * 
                 * @note Évite la conversion view -> NkString pour efficacité.
                 */
                int32 Compare(NkStringView other) const noexcept;

                /**
                 * @brief Compare avec une chaîne C-style
                 * 
                 * @param str Chaîne C à comparer
                 * @return Résultat de comparaison lexicographique
                 * 
                 * @note Gère str == nullptr comme chaîne vide.
                 */
                int32 Compare(const char* str) const noexcept;

            // =================================================================
            // PRÉDICATS : PREFIXE, SUFFIXE, CONTENU
            // =================================================================
            public:

                /**
                 * @brief Vérifie si la chaîne commence par un préfixe donné
                 * 
                 * @param prefix Vue représentant le préfixe attendu
                 * @return true si les premiers caractères correspondent exactement
                 * 
                 * @note Comparaison binaire rapide.
                 *       Retourne false si prefix.Length() > this->Length().
                 */
                bool StartsWith(NkStringView prefix) const noexcept;

                /**
                 * @brief Vérifie si la chaîne commence par un caractère donné
                 * 
                 * @param ch Caractère à tester en première position
                 * @return true si Length() > 0 && Data()[0] == ch
                 */
                bool StartsWith(char ch) const noexcept;

                /**
                 * @brief Vérifie le préfixe avec chaîne C (wrapper)
                 * 
                 * @param prefix Chaîne C représentant le préfixe attendu
                 * @return true si préfixe match, false si prefix == nullptr
                 * 
                 * @note Wrapper pratique pour interop C-style.
                 */
                bool StartsWith(const char* prefix) const noexcept {
                    return prefix ? StartsWith(NkStringView(prefix)) : false;
                }

                /**
                 * @brief Vérifie si la chaîne se termine par un suffixe donné
                 * 
                 * @param suffix Vue représentant le suffixe attendu
                 * @return true si les derniers caractères correspondent exactement
                 * 
                 * @note Compare à partir de (Length() - suffix.Length()).
                 *       Retourne false si suffix est plus long que this.
                 */
                bool EndsWith(NkStringView suffix) const noexcept;

                /**
                 * @brief Vérifie si la chaîne se termine par un caractère donné
                 * 
                 * @param ch Caractère à tester en dernière position
                 * @return true si Length() > 0 && Data()[Length()-1] == ch
                 */
                bool EndsWith(char ch) const noexcept;

                /**
                 * @brief Vérifie le suffixe avec chaîne C (wrapper)
                 * 
                 * @param suffix Chaîne C représentant le suffixe attendu
                 * @return true si suffixe match, false si suffix == nullptr
                 */
                bool EndsWith(const char* suffix) const noexcept {
                    return suffix ? EndsWith(NkStringView(suffix)) : false;
                }

                /**
                 * @brief Vérifie si la chaîne contient une sous-chaîne donnée
                 * 
                 * @param str Sous-chaîne à rechercher
                 * @return true si Find(str) != npos
                 * 
                 * @note Implémentation naïve O(n*m) : suffisante pour la plupart des cas.
                 *       Pour performances critiques sur grands textes, envisager
                 *       des algorithmes avancés (Boyer-Moore, etc.).
                 */
                bool Contains(NkStringView str) const noexcept;

                /**
                 * @brief Vérifie si la chaîne contient un caractère donné
                 * 
                 * @param ch Caractère à rechercher
                 * @return true si le caractère est présent au moins une fois
                 * 
                 * @note Parcours linéaire optimisé : O(n) worst-case.
                 */
                bool Contains(char ch) const noexcept;

                /**
                 * @brief Vérifie la présence d'une sous-chaîne avec chaîne C (wrapper)
                 * 
                 * @param str Chaîne C à rechercher
                 * @return true si trouvée, false si str == nullptr
                 */
                bool Contains(const char* str) const noexcept {
                    return str ? Contains(NkStringView(str)) : false;
                }

            // =================================================================
            // RECHERCHE DE POSITIONS (Find / RFind)
            // =================================================================
            public:

                /**
                 * @brief Recherche la première occurrence d'une sous-chaîne
                 * 
                 * @param str Sous-chaîne à rechercher
                 * @param pos Position de départ pour la recherche (défaut: 0)
                 * @return Index de la première occurrence, ou npos si non trouvé
                 * 
                 * @note Recherche depuis pos jusqu'à la fin.
                 *       Complexité O(n*m) dans le pire cas.
                 */
                SizeType Find(NkStringView str, SizeType pos = 0) const noexcept;

                /**
                 * @brief Recherche la première occurrence d'un caractère
                 * 
                 * @param ch Caractère à rechercher
                 * @param pos Position de départ (défaut: 0)
                 * @return Index de la première occurrence, ou npos si absent
                 * 
                 * @note Parcours linéaire optimisé : souvent plus rapide que Find(str).
                 */
                SizeType Find(char ch, SizeType pos = 0) const noexcept;

                /**
                 * @brief Recherche avec chaîne C (wrapper)
                 * 
                 * @param str Chaîne C à rechercher
                 * @param pos Position de départ
                 * @return Index de la première occurrence, ou npos si str==nullptr ou non trouvé
                 */
                SizeType Find(const char* str, SizeType pos = 0) const noexcept {
                    return str ? Find(NkStringView(str), pos) : npos;
                }

                /**
                 * @brief Recherche la dernière occurrence d'une sous-chaîne
                 * 
                 * @param str Sous-chaîne à rechercher
                 * @param pos Position limite pour recherche arrière (défaut: npos = fin)
                 * @return Index de la dernière occurrence, ou npos si non trouvé
                 * 
                 * @note Recherche depuis min(pos, Length()-1) vers le début.
                 */
                SizeType RFind(NkStringView str, SizeType pos = npos) const noexcept;

                /**
                 * @brief Recherche la dernière occurrence d'un caractère
                 * 
                 * @param ch Caractère à rechercher
                 * @param pos Position limite pour recherche arrière
                 * @return Index de la dernière occurrence, ou npos si absent
                 */
                SizeType RFind(char ch, SizeType pos = npos) const noexcept;

                /**
                 * @brief Recherche arrière avec chaîne C (wrapper)
                 * 
                 * @param str Chaîne C à rechercher
                 * @param pos Position limite pour recherche arrière
                 * @return Index de la dernière occurrence, ou npos
                 */
                SizeType RFind(const char* str, SizeType pos = npos) const noexcept {
                    return str ? RFind(NkStringView(str), pos) : npos;
                }

            // =================================================================
            // CONVERSIONS ET INTEROP
            // =================================================================
            public:

                /**
                 * @brief Conversion implicite vers NkStringView
                 * 
                 * @return Vue en lecture seule sur les données de cette instance
                 * 
                 * @note Permet de passer une NkString là où une NkStringView est attendue.
                 *       Aucune copie : la vue référence les données internes.
                 *       @warning La vue devient invalide si cette instance est modifiée.
                 */
                operator NkStringView() const noexcept;

                /**
                 * @brief Conversion explicite vers NkStringView
                 * 
                 * @return Vue en lecture seule sur les données de cette instance
                 * 
                 * @note Équivalent à l'opérateur de conversion, mais plus explicite.
                 *       Utile pour clarifier l'intention dans le code.
                 */
                NkStringView View() const noexcept;

            // =================================================================
            // ITÉRATEURS (Compatibilité STL)
            // =================================================================
            public:

                /**
                 * @brief Itérateur modifiable vers le premier élément
                 * 
                 * @return Pointeur modifiable vers Data()
                 */
                char* begin() noexcept;

                /**
                 * @brief Itérateur constant vers le premier élément
                 * 
                 * @return Pointeur constant vers Data()
                 */
                const char* begin() const noexcept;

                /**
                 * @brief Itérateur modifiable vers après le dernier élément
                 * 
                 * @return Pointeur modifiable vers Data() + Length()
                 */
                char* end() noexcept;

                /**
                 * @brief Itérateur constant vers après le dernier élément
                 * 
                 * @return Pointeur constant vers Data() + Length()
                 */
                const char* end() const noexcept;

            // =================================================================
            // GESTION D'ALLOCATEUR
            // =================================================================
            public:

                /**
                 * @brief Retourne une référence vers l'allocateur utilisé
                 * 
                 * @return Référence constante vers l'allocateur interne
                 * 
                 * @note Utile pour :
                 *       - Créer d'autres objets avec le même allocateur
                 *       - Debugging de stratégie mémoire
                 *       - Pooling d'objets cohérents
                 */
                memory::NkIAllocator& GetAllocator() const noexcept;

            // =================================================================
            // RECHERCHE AVANCÉE (FindFirstOf / FindLastOf / NotOf)
            // =================================================================
            public:

                /**
                 * @brief Trouve le premier caractère appartenant à un ensemble
                 * 
                 * @param chars Vue des caractères à rechercher
                 * @param pos Position de départ
                 * @return Index du premier match, ou npos si aucun
                 * 
                 * @note Utile pour : tokenization, parsing de délimiteurs multiples.
                 */
                SizeType FindFirstOf(NkStringView chars, SizeType pos = 0) const noexcept;

                /**
                 * @brief Trouve le dernier caractère appartenant à un ensemble
                 * 
                 * @param chars Vue des caractères à rechercher
                 * @param pos Position limite pour recherche arrière
                 * @return Index du dernier match, ou npos si aucun
                 */
                SizeType FindLastOf(NkStringView chars, SizeType pos = npos) const noexcept;

                /**
                 * @brief Trouve le premier caractère N'APPARTENANT PAS à un ensemble
                 * 
                 * @param chars Vue des caractères à exclure
                 * @param pos Position de départ
                 * @return Index du premier caractère "extérieur", ou npos
                 * 
                 * @note Utile pour : sauter des espaces, ignorer des préfixes.
                 */
                SizeType FindFirstNotOf(NkStringView chars, SizeType pos = 0) const noexcept;

                /**
                 * @brief Trouve le dernier caractère N'APPARTENANT PAS à un ensemble
                 * 
                 * @param chars Vue des caractères à exclure
                 * @param pos Position limite pour recherche arrière
                 * @return Index du dernier caractère "extérieur", ou npos
                 */
                SizeType FindLastNotOf(NkStringView chars, SizeType pos = npos) const noexcept;

            // =================================================================
            // TRANSFORMATIONS IN-PLACE
            // =================================================================
            public:

                /**
                 * @brief Convertit la chaîne en minuscules (ASCII) in-place
                 * 
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Modifie directement le contenu : pas de copie.
                 *       Conversion ASCII uniquement : pour Unicode, utiliser ICU.
                 */
                NkString& ToLower();

                /**
                 * @brief Convertit la chaîne en majuscules (ASCII) in-place
                 * 
                 * @return Référence vers this pour chaînage
                 */
                NkString& ToUpper();

                /**
                 * @brief Supprime les espaces blancs aux deux extrémités in-place
                 * 
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Décale les données pour compacter : peut être coûteux
                 *       sur de grandes chaînes. Préférer NkStringView::Trimmed()
                 *       pour une version non-destructive.
                 */
                NkString& Trim();

                /**
                 * @brief Supprime les espaces blancs à gauche uniquement in-place
                 * 
                 * @return Référence vers this pour chaînage
                 */
                NkString& TrimLeft();

                /**
                 * @brief Supprime les espaces blancs à droite uniquement in-place
                 * 
                 * @return Référence vers this pour chaînage
                 */
                NkString& TrimRight();

            // =================================================================
            // CONVERSIONS NUMÉRIQUES
            // =================================================================
            public:

                /**
                 * @brief Tente de parser la chaîne comme entier 32 bits signé
                 * 
                 * @param out Référence pour stocker le résultat
                 * @return true si parsing réussi, false si format invalide
                 * 
                 * @note Accepte : espaces initiaux, signe +/-, chiffres décimaux.
                 *       Rejette tout caractère non-numérique après le nombre.
                 */
                bool ToInt(int32& out) const noexcept;

                /**
                 * @brief Tente de parser comme flottant 32 bits
                 * 
                 * @param out Référence pour le résultat float32
                 * @return true si succès, false si format invalide
                 * 
                 * @note Accepte notation décimale et scientifique (1.23e-4)
                 */
                bool ToFloat(float32& out) const noexcept;

                /**
                 * @brief Tente de parser comme entier 64 bits signé
                 * 
                 * @param out Référence pour le résultat int64
                 * @return true si succès, false sinon
                 */
                bool ToInt64(int64& out) const noexcept;

                /**
                 * @brief Tente de parser comme entier non-signé 32 bits
                 * 
                 * @param out Référence pour le résultat uint32
                 * @return true si succès, false si négatif ou format invalide
                 */
                bool ToUInt(uint32& out) const noexcept;

                /**
                 * @brief Tente de parser comme entier non-signé 64 bits
                 * 
                 * @param out Référence pour le résultat uint64
                 * @return true si succès, false sinon
                 */
                bool ToUInt64(uint64& out) const noexcept;

                /**
                 * @brief Tente de parser comme flottant 64 bits
                 * 
                 * @param out Référence pour le résultat float64
                 * @return true si succès, false sinon
                 */
                bool ToDouble(float64& out) const noexcept;

                /**
                 * @brief Tente de parser comme valeur booléenne texte
                 * 
                 * @param out Référence pour le résultat bool
                 * @return true si parsing réussi
                 * 
                 * @note Accepte : "true"/"false", "1"/"0", "yes"/"no" (case-insensitive)
                 */
                bool ToBool(bool& out) const noexcept;

            // =================================================================
            // CONVERSIONS AVEC VALEURS PAR DÉFAUT (Fallback)
            // =================================================================
            public:

                /**
                 * @brief Parse en int32 ou retourne la valeur par défaut
                 * 
                 * @param defaultValue Valeur à retourner en cas d'échec
                 * @return Résultat du parsing ou defaultValue
                 * 
                 * @note Évite la gestion explicite de bool de retour.
                 *       Utile pour les configurations, paramètres optionnels.
                 */
                int32 ToInt32(int32 defaultValue = 0) const noexcept;

                /**
                 * @brief Parse en float32 avec fallback
                 * 
                 * @param defaultValue Valeur par défaut en cas d'échec
                 * @return Résultat ou defaultValue
                 */
                float32 ToFloat32(float32 defaultValue = 0.0f) const noexcept;

            // =================================================================
            // PRÉDICATS DE CONTENU (Tests sémantiques)
            // =================================================================
            public:

                /**
                 * @brief Vérifie si la chaîne ne contient que des chiffres décimaux
                 * 
                 * @return true si tous les caractères sont '0'-'9'
                 */
                bool IsDigits() const noexcept;

                /**
                 * @brief Vérifie si la chaîne ne contient que des lettres ASCII
                 * 
                 * @return true si tous les caractères sont A-Z ou a-z
                 */
                bool IsAlpha() const noexcept;

                /**
                 * @brief Vérifie si la chaîne est alphanumérique ASCII
                 * 
                 * @return true si tous les caractères sont lettres ou chiffres
                 */
                bool IsAlphaNumeric() const noexcept;

                /**
                 * @brief Vérifie si la chaîne ne contient que des espaces blancs
                 * 
                 * @return true si tous les caractères sont whitespace
                 */
                bool IsWhitespace() const noexcept;

                /**
                 * @brief Vérifie si la chaîne représente un nombre valide
                 * 
                 * @return true si format entier ou flottant reconnu
                 * 
                 * @note Accepte : signe, décimales, exposant scientifique
                 */
                bool IsNumeric() const noexcept;

                /**
                 * @brief Vérifie si la chaîne représente un entier valide
                 * 
                 * @return true si format entier signé/non-signé reconnu
                 */
                bool IsInteger() const noexcept;

            // =================================================================
            // MANIPULATIONS AVANCÉES
            // =================================================================
            public:

                /**
                 * @brief Inverse l'ordre des caractères in-place
                 * 
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Ex: "abc" -> "cba"
                 *       Complexité O(n/2) : échange symétrique
                 */
                NkString& Reverse();

                /**
                 * @brief Met la première lettre en majuscule, le reste en minuscules
                 * 
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Ex: "hELLO" -> "Hello"
                 *       Conversion ASCII uniquement
                 */
                NkString& Capitalize();

                /**
                 * @brief Met chaque mot en Title Case (majuscule après espace)
                 * 
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Ex: "hello world" -> "Hello World"
                 *       Délimiteurs de mots : espaces, tabulations
                 */
                NkString& TitleCase();

                /**
                 * @brief Supprime toutes les occurrences des caractères spécifiés
                 * 
                 * @param charsToRemove Vue des caractères à éliminer
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Compacte la chaîne en place : peut être coûteux.
                 *       Ex: "a1b2c3".RemoveChars("123") -> "abc"
                 */
                NkString& RemoveChars(NkStringView charsToRemove);

                /**
                 * @brief Supprime toutes les occurrences d'un caractère spécifique
                 * 
                 * @param ch Caractère à supprimer
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Version optimisée de RemoveChars pour un seul caractère.
                 */
                NkString& RemoveAll(char ch);

            // =================================================================
            // FORMATAGE : STYLE PRINTF (VARIABLE ARGUMENTS)
            // =================================================================
            public:

                /**
                 * @brief Formatage style printf avec arguments variables
                 * 
                 * @param format Chaîne de format avec spécificateurs (%s, %d, etc.)
                 * @param ... Arguments variables correspondant aux spécificateurs
                 * @return Nouvelle instance NkString formatée
                 * 
                 * @note Délègue à une fonction interne NkFormatf.
                 *       Syntaxe compatible vsnprintf : %s, %d, %f, %.2f, etc.
                 *       @warning Ne pas passer de NkString directement : utiliser CStr().
                 */
                static NkString Format(const char* format, ...);

                /**
                 * @brief Formatage style printf avec va_list
                 * 
                 * @param format Chaîne de format avec spécificateurs
                 * @param args Liste d'arguments de type va_list
                 * @return Nouvelle instance NkString formatée
                 * 
                 * @note Version pour wrappers ou fonctions forwardant les arguments.
                 *       Ne modifie pas args : peut être réutilisé après appel.
                 */
                static NkString VFormat(const char* format, va_list args);

                /**
                 * @brief Alias pour Format() (nom alternatif)
                 * 
                 * @param format Chaîne de format
                 * @param ... Arguments variables
                 * @return Chaîne formatée
                 * 
                 * @note Fmtf = Format : deux noms pour préférences de style.
                 */
                static NkString Fmtf(const char* format, ...);

                /**
                 * @brief Alias pour VFormat() (nom alternatif)
                 * 
                 * @param format Chaîne de format
                 * @param args Liste va_list
                 * @return Chaîne formatée
                 */
                static NkString VFmtf(const char* format, va_list args);

            // =================================================================
            // FORMATAGE : STYLE MODERNE {PLACEHOLDER}
            // =================================================================
            public:

                /**
                 * @brief Formatage moderne avec placeholders {index:options}
                 * 
                 * @tparam Args Types des arguments à formatter (déduits automatiquement)
                 * @param format Chaîne de format avec placeholders {0}, {1:w=8}, etc.
                 * @param args Arguments à insérer dans les placeholders
                 * @return Nouvelle instance NkString formatée
                 * 
                 * @note Syntaxe : {index[:options]}
                 *       Options supportées : w=largeur, .précision, >/<^ alignement
                 *       Ex: NkString::Fmt("{0:w=8 >} = {1:.3}", 42, 3.14159)
                 *           -> "      42 = 3.142"
                 * 
                 * @warning Requiert l'inclusion de "NKContainers/String/NkFormat.h"
                 *          pour la définition du corps template.
                 *          Ce header déclare uniquement l'interface.
                 */
                template<typename... Args>
                static NkString Fmt(const char* format, const Args&... args);

            // =================================================================
            // UTILITAIRES : HASH
            // =================================================================
            public:

                /**
                 * @brief Calcule un hash 64 bits de la chaîne
                 * 
                 * @return Valeur de hash uint64
                 * 
                 * @note Algorithme interne optimisé pour distribution uniforme.
                 *       Non-cryptographique : pour tables de hachage uniquement.
                 *       Identique à NkStringView(data, length).Hash().
                 */
                uint64 Hash() const noexcept;

            // =================================================================
            // MEMBRES PRIVÉS : GESTION INTERNE ET SSO
            // =================================================================
            private:

                /// Pointeur vers l'allocateur utilisé (nullptr = allocateur par défaut)
                memory::NkIAllocator* mAllocator;

                /// Nombre actuel de caractères dans la chaîne (exclut null-terminator)
                SizeType mLength;

                /// Capacité totale allouée (en caractères, exclut null-terminator)
                SizeType mCapacity;

                /// Union pour Small String Optimization : soit heap, soit buffer inline
                union {
                    /// Pointeur vers données heap (si !IsSSO())
                    char* mHeapData;

                    /// Buffer inline pour petites chaînes (si IsSSO())
                    /// Taille : SSO_SIZE caractères + 1 pour null-terminator
                    char mSSOData[SSO_SIZE + 1];
                };

                /**
                 * @brief Vérifie si le stockage SSO est actif
                 * 
                 * @return true si les données sont dans mSSOData, false si heap
                 * 
                 * @note Implémentation : utilise un bit de drapeau dans mCapacity
                 *       ou un sentinel value, selon l'optimisation plateforme.
                 */
                bool IsSSO() const noexcept;

                /**
                 * @brief Active ou désactive le mode SSO
                 * 
                 * @param sso true pour activer SSO, false pour heap
                 * 
                 * @note Modifie le drapeau interne sans toucher aux données.
                 *       À appeler uniquement lors de transitions SSO<->heap.
                 */
                void SetSSO(bool sso) noexcept;

                /**
                 * @brief Retourne le pointeur vers les données (modifiable)
                 * 
                 * @return Pointeur vers mHeapData ou mSSOData selon IsSSO()
                 * 
                 * @note Abstraction pour accéder aux données sans vérifier SSO manuellement.
                 */
                char* GetData() noexcept;

                /**
                 * @brief Retourne le pointeur vers les données (const)
                 * 
                 * @return Pointeur constant vers les données
                 */
                const char* GetData() const noexcept;

                /**
                 * @brief Alloue un buffer heap de capacité donnée
                 * 
                 * @param capacity Nouvelle capacité souhaitée (en caractères)
                 * 
                 * @note Utilise mAllocator si défini, sinon allocateur par défaut.
                 *       Ne copie pas les données existantes : appeler après
                 *       avoir sauvegardé/converti le contenu si nécessaire.
                 */
                void AllocateHeap(SizeType capacity);

                /**
                 * @brief Libère le buffer heap si actif
                 * 
                 * @note No-op si IsSSO() == true.
                 *       Met mHeapData à nullptr après libération.
                 */
                void DeallocateHeap();

                /**
                 * @brief Migre les données du mode SSO vers heap
                 * 
                 * @param newCapacity Capacité cible pour le nouveau buffer heap
                 * 
                 * @note Copie les données de mSSOData vers mHeapData.
                 *       Met à jour mCapacity et désactive le flag SSO.
                 *       Appelé automatiquement quand Length() > SSO_SIZE.
                 */
                void MoveToHeap(SizeType newCapacity);

                /**
                 * @brief Garantit que la capacité peut accueillir des données supplémentaires
                 * 
                 * @param additionalSize Nombre de caractères supplémentaires prévus
                 * 
                 * @note Si Capacity() >= Length() + additionalSize : no-op.
                 *       Sinon : calcule nouvelle capacité avec stratégie de croissance
                 *       et appelle AllocateHeap/MoveToHeap si nécessaire.
                 */
                void GrowIfNeeded(SizeType additionalSize);

                /**
                 * @brief Calcule la nouvelle capacité selon stratégie de croissance
                 * 
                 * @param current Capacité actuelle
                 * @param needed Capacité minimale requise
                 * @return Nouvelle capacité recommandée (>= needed)
                 * 
                 * @note Stratégie typique : croissance géométrique (x1.5 ou x2)
                 *       pour amortir le coût des réallocations multiples.
                 *       Plafonnée à npos - 1 pour éviter overflow.
                 */
                static SizeType CalculateGrowth(SizeType current, SizeType needed);
        };

        // =====================================================================
        // OPÉRATEURS NON-MEMBRES : CONCATÉNATION (+)
        // =====================================================================

        /**
         * @brief Concatène deux NkString
         * 
         * @param lhs Opérande gauche
         * @param rhs Opérande droite
         * @return Nouvelle instance contenant lhs + rhs
         * 
         * @note Crée une nouvelle chaîne : n'affecte pas lhs ni rhs.
         *       Optimisé : pré-calcule la taille totale pour une seule allocation.
         */
        NkString operator+(const NkString& lhs, const NkString& rhs);

        /**
         * @brief Concatène NkString + chaîne C
         * 
         * @param lhs Instance NkString à gauche
         * @param rhs Chaîne C à droite
         * @return Nouvelle instance contenant lhs + rhs
         */
        NkString operator+(const NkString& lhs, const char* rhs);

        /**
         * @brief Concatène chaîne C + NkString
         * 
         * @param lhs Chaîne C à gauche
         * @param rhs Instance NkString à droite
         * @return Nouvelle instance contenant lhs + rhs
         */
        NkString operator+(const char* lhs, const NkString& rhs);

        /**
         * @brief Concatène NkString + caractère unique
         * 
         * @param lhs Instance NkString à gauche
         * @param rhs Caractère à ajouter à droite
         * @return Nouvelle instance contenant lhs + rhs
         */
        NkString operator+(const NkString& lhs, char rhs);

        /**
         * @brief Concatène caractère unique + NkString
         * 
         * @param lhs Caractère à gauche
         * @param rhs Instance NkString à droite
         * @return Nouvelle instance contenant lhs + rhs
         */
        NkString operator+(char lhs, const NkString& rhs);

        // =====================================================================
        // OPÉRATEURS NON-MEMBRES : COMPARAISON
        // =====================================================================

        /**
         * @brief Teste l'égalité binaire entre deux NkString
         * 
         * @param lhs Opérande gauche
         * @param rhs Opérande droite
         * @return true si mêmes données et même longueur
         * 
         * @note Comparaison rapide : vérifie d'abord Length(), puis memcmp si égal.
         */
        bool operator==(const NkString& lhs, const NkString& rhs) noexcept;

        /**
         * @brief Teste l'égalité NkString vs chaîne C
         * 
         * @param lhs Instance NkString
         * @param rhs Chaîne C à comparer
         * @return true si égalité binaire
         * 
         * @note Gère rhs == nullptr comme chaîne vide.
         */
        bool operator==(const NkString& lhs, const char* rhs) noexcept;

        /**
         * @brief Teste la différence entre deux NkString
         * 
         * @param lhs Opérande gauche
         * @param rhs Opérande droite
         * @return true si contenus différents
         */
        bool operator!=(const NkString& lhs, const NkString& rhs) noexcept;

        /**
         * @brief Teste la différence NkString vs chaîne C
         * 
         * @param lhs Instance NkString
         * @param rhs Chaîne C
         * @return true si inégalité
         */
        bool operator!=(const NkString& lhs, const char* rhs) noexcept;

        /**
         * @brief Comparaison inférieure lexicographique
         * 
         * @param lhs Opérande gauche
         * @param rhs Opérande droite
         * @return true si lhs < rhs (ordre binaire)
         */
        bool operator<(const NkString& lhs, const NkString& rhs) noexcept;

        /**
         * @brief Comparaison inférieure ou égale lexicographique
         * 
         * @param lhs Opérande gauche
         * @param rhs Opérande droite
         * @return true si lhs <= rhs
         */
        bool operator<=(const NkString& lhs, const NkString& rhs) noexcept;

        /**
         * @brief Comparaison supérieure lexicographique
         * 
         * @param lhs Opérande gauche
         * @param rhs Opérande droite
         * @return true si lhs > rhs
         */
        bool operator>(const NkString& lhs, const NkString& rhs) noexcept;

        /**
         * @brief Comparaison supérieure ou égale lexicographique
         * 
         * @param lhs Opérande gauche
         * @param rhs Opérande droite
         * @return true si lhs >= rhs
         */
        bool operator>=(const NkString& lhs, const NkString& rhs) noexcept;

    } // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_STRING_NKSTRING_H_INCLUDED

// -----------------------------------------------------------------------------
// EXEMPLES D'UTILISATION
// -----------------------------------------------------------------------------
/*
    // =====================================================================
    // Exemple 1 : Construction et manipulation de base
    // =====================================================================
    {
        // Construction depuis différents types
        nkentseu::NkString s1;                           // ""
        nkentseu::NkString s2("Hello");                  // "Hello"
        nkentseu::NkString s3("World", 3);               // "Wor"
        nkentseu::NkString s4(5, '*');                   // "*****"
        nkentseu::NkString s5 = s2 + " " + s2;           // "Hello Hello"
        
        // Modification in-place
        s1.Append("Start");                              // "Start"
        s1 += " - Middle";                               // "Start - Middle"
        s1.PushBack('!');                                // "Start - Middle!"
        s1.Insert(6, "IMPORTANT ");                      // "Start IMPORTANT - Middle!"
        
        // Extraction et transformation
        auto sub = s1.SubStr(6, 9);                      // "IMPORTANT"
        auto upper = sub; upper.ToUpper();               // "IMPORTANT"
        auto reversed = sub; reversed.Reverse();         // "TNATROPMI"
        
        // Recherche et prédicats
        if (s1.Contains("Middle") && s1.EndsWith('!')) {
            auto pos = s1.Find("IMPORTANT");             // pos = 6
            s1.Erase(pos, 9);                            // Supprime "IMPORTANT "
        }
    }

    // =====================================================================
    // Exemple 2 : Formatage moderne avec placeholders
    // =====================================================================
    {
        // Formatage style moderne (nécessite #include "NkFormat.h" pour le corps)
        auto report = nkentseu::NkString::Fmt(
            "User: {0:w=10} | Score: {1:.2f} | Level: {2}",
            "Alice", 98.7654, 42
        );
        // Résultat : "User: Alice      | Score: 98.77 | Level: 42"
        
        // Alignement et padding
        auto table = nkentseu::NkString::Fmt(
            "{0:<10} | {1:>8} | {2:^12}",
            "Name", "Age", "Status"
        );
        // Résultat : "Name       |      Age |    Status    "
        
        // Formatage printf-style (alternative)
        auto legacy = nkentseu::NkString::Format(
            "Value: %d, Rate: %.3f, Text: %s",
            123, 4.56789, "example"
        );
        // Résultat : "Value: 123, Rate: 4.568, Text: example"
    }

    // =====================================================================
    // Exemple 3 : Gestion d'allocateur personnalisé pour pooling
    // =====================================================================
    {
        // Supposons un allocateur pool pour petites chaînes fréquentes
        class StringPoolAllocator : public nkentseu::memory::NkIAllocator {
            // ... implémentation spécifique ...
        };
        
        StringPoolAllocator poolAlloc;
        
        // Création de nombreuses petites chaînes avec allocateur pool
        std::vector<nkentseu::NkString> cache;
        cache.reserve(1000);
        
        for (int i = 0; i < 1000; ++i) {
            // Toutes utilisent le même allocateur : mémoire contiguë, moins de fragmentation
            cache.emplace_back(nkentseu::NkString("key_", poolAlloc));
            cache.back().Append(std::to_string(i).c_str());
        }
        
        // Libération groupée possible si l'allocateur supporte le reset
        // poolAlloc.Reset();  // Libère tout le pool en O(1)
    }

    // =====================================================================
    // Exemple 4 : Parsing et validation d'entrée utilisateur
    // =====================================================================
    {
        bool ParseAndValidateConfig(nkentseu::NkStringView input, Config& out) {
            auto trimmed = input.Trimmed();
            
            // Ignorer commentaires et lignes vides
            if (trimmed.Empty() || trimmed.StartsWith('#')) {
                return false;
            }
            
            // Parser clé=valeur
            auto eqPos = trimmed.Find('=');
            if (eqPos == nkentseu::NkStringView::npos) {
                return false;
            }
            
            auto key = trimmed.Left(eqPos).Trimmed();
            auto value = trimmed.SubStr(eqPos + 1).Trimmed();
            
            // Validation selon la clé
            if (key.Equals("timeout")) {
                return value.ToInt32(out.timeout);  // Retourne false si invalide
            }
            if (key.Equals("enable_feature")) {
                return value.ToBool(out.featureEnabled);
            }
            if (key.Equals("max_retries")) {
                int32 retries = 0;
                if (value.ToInt32(retries) && retries >= 0 && retries <= 10) {
                    out.maxRetries = static_cast<uint8>(retries);
                    return true;
                }
                return false;
            }
            
            return false;  // Clé inconnue
        }
    }

    // =====================================================================
    // Exemple 5 : Optimisation avec SSO et avoidance d'allocations
    // =====================================================================
    {
        void ProcessLogEntry(const char* level, const char* message) {
            // Utilisation de NkStringView pour éviter copies inutiles
            nkentseu::NkStringView levelView(level);
            nkentseu::NkStringView msgView(message);
            
            // Construction finale uniquement si nécessaire (ex: écriture fichier)
            if (ShouldLog(levelView)) {
                // SSO : si le message est court (<24 chars), aucune allocation heap !
                nkentseu::NkString logLine;
                logLine.Append("[")
                       .Append(levelView)
                       .Append("] ")
                       .Append(msgView);
                
                WriteToLog(logLine.CStr());  // Interop C-style
            }
        }
        
        // Bénéfice : pour messages courts fréquents, 0 allocation dynamique
        //           pour messages longs, allocation unique (pas de réallocation grâce à Reserve si estimé)
    }

    // =====================================================================
    // Exemple 6 : Hash pour conteneurs associatifs
    // =====================================================================
    {
        // Structure de hash pour utiliser NkString comme clé dans HashMap
        struct NkStringHash {
            size_t operator()(const nkentseu::NkString& str) const noexcept {
                return static_cast<size_t>(str.Hash());
            }
        };
        
        // Structure d'égalité
        struct NkStringEqual {
            bool operator()(
                const nkentseu::NkString& a, 
                const nkentseu::NkString& b
            ) const noexcept {
                return a.Equals(b);
            }
        };
        
        // Utilisation (pseudo-code pour HashMap custom)
        // HashMap<nkentseu::NkString, ValueType, NkStringHash, NkStringEqual> configMap;
        
        // Insertion et lookup efficaces
        // configMap.Insert("database_url"_nv, "postgres://...");
        // auto* value = configMap.Find("database_url");  // Lookup sans allocation temporaire
    }

    // =====================================================================
    // Exemple 7 : Bonnes pratiques et pièges à éviter
    // =====================================================================
    {
        // ✅ BON : Réserver la capacité avant boucle d'ajouts
        nkentseu::NkString BuildCsv(const std::vector<const char*>& fields) {
            nkentseu::NkString result;
            
            // Estimation : somme des longueurs + séparateurs
            size_t estimated = 0;
            for (const char* f : fields) {
                estimated += f ? strlen(f) + 1 : 1;  // +1 pour virgule
            }
            result.Reserve(estimated);  // Évite N réallocations
            
            for (size_t i = 0; i < fields.size(); ++i) {
                if (i > 0) result += ',';
                if (fields[i]) result += fields[i];
            }
            return result;
        }
        
        // ❌ À ÉVITER : Concaténation naïve en boucle sans Reserve
        // nkentseu::NkString bad;
        // for (int i = 0; i < 1000; ++i) {
        //     bad += "chunk";  // Peut déclencher ~10 réallocations (1,2,4,8,16...512,1024)
        // }
        
        // ✅ BON : Utiliser NkStringView pour paramètres, NkString pour retour possédant
        nkentseu::NkString Transform(nkentseu::NkStringView input) {
            if (input.StartsWith("prefix_")) {
                return input.SubStr(7).ToUpper().ToString();  // Copie uniquement si nécessaire
            }
            return input.ToString();  // Copie explicite
        }
        
        // ❌ À ÉVITER : Retourner une vue sur donnée locale
        // nkentseu::NkStringView BadReturn() {
        //     nkentseu::NkString local = "temp";
        //     return local.View();  // ⚠️ Vue invalide après retour de fonction !
        // }
    }
*/