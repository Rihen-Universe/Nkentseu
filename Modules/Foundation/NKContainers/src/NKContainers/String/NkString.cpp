// -----------------------------------------------------------------------------
// FICHIER: Core\NKContainers\src\NKContainers\String\NkString.cpp
// DESCRIPTION: Implémentation de la classe NkString avec Small String Optimization (SSO)
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

// -------------------------------------------------------------------------
// Inclusion de l'en-tête correspondant
// -------------------------------------------------------------------------
#include "NkString.h"

// -------------------------------------------------------------------------
// Inclusions des utilitaires de chaînes
// -------------------------------------------------------------------------
#include "NkStringUtils.h"
#include "NkStringHash.h"
#include "NKMemory/NkFunction.h"

// -------------------------------------------------------------------------
// Inclusions standard pour le formatage printf-style
// -------------------------------------------------------------------------
#include <cstdarg>

// -------------------------------------------------------------------------
// Namespace principal du projet
// -------------------------------------------------------------------------
namespace nkentseu {

    // =====================================================================
    // SECTION : CONSTRUCTEURS
    // =====================================================================

    /**
     * @brief Constructeur par défaut : chaîne vide avec allocateur par défaut
     * 
     * Initialise une NkString vide en mode SSO (Small String Optimization).
     * Aucune allocation heap n'est effectuée : le buffer interne SSO est utilisé.
     * 
     * @note Complexité : O(1), constante
     *       Le null-terminator est placé en position 0 du buffer SSO.
     */
    NkString::NkString()
        : mAllocator(&memory::NkGetDefaultAllocator())
        , mLength(0)
        , mCapacity(SSO_SIZE) {
        mSSOData[0] = '\0';
        SetSSO(true);
    }

    /**
     * @brief Constructeur avec allocateur personnalisé
     * 
     * @param allocator Référence vers l'allocateur à utiliser pour les allocations futures
     * 
     * @note Initialise la chaîne vide en mode SSO.
     *       L'allocateur passé doit rester valide pendant toute la durée de vie de l'instance.
     */
    NkString::NkString(memory::NkIAllocator& allocator)
        : mAllocator(&allocator)
        , mLength(0)
        , mCapacity(SSO_SIZE) {
        mSSOData[0] = '\0';
        SetSSO(true);
    }

    /**
     * @brief Constructeur depuis une chaîne C terminée par null
     * 
     * @param str Pointeur vers une chaîne C-style (const char*)
     * 
     * @note Copie les données de str dans cette instance.
     *       Si str est nullptr ou vide : initialise une chaîne vide en mode SSO.
     *       Si longueur <= SSO_SIZE : utilise le buffer SSO.
     *       Si longueur > SSO_SIZE : alloue sur le heap via MoveToHeap().
     */
    NkString::NkString(const char* str)
        : mAllocator(&memory::NkGetDefaultAllocator())
        , mLength(0)
        , mCapacity(SSO_SIZE) {
        mSSOData[0] = '\0';
        SetSSO(true);
        if (str) {
            SizeType len = string::NkLength(str);
            if (len > 0) {
                if (len <= SSO_SIZE) {
                    memory::NkMemCopy(mSSOData, str, len);
                    mSSOData[len] = '\0';
                    mLength = len;
                } else {
                    MoveToHeap(len);
                    memory::NkMemCopy(mHeapData, str, len);
                    mHeapData[len] = '\0';
                    mLength = len;
                }
            }
        }
    }

    /**
     * @brief Constructeur depuis C-string avec allocateur personnalisé
     * 
     * @param str Pointeur vers chaîne C terminée par null
     * @param allocator Référence vers l'allocateur à utiliser
     * 
     * @note Délègue d'abord au constructeur avec allocateur, puis appelle Append().
     *       Permet de spécifier explicitement la stratégie d'allocation mémoire.
     */
    NkString::NkString(const char* str, memory::NkIAllocator& allocator)
        : NkString(allocator) {
        if (str) {
            Append(str);
        }
    }

    /**
     * @brief Constructeur depuis buffer + longueur explicite
     * 
     * @param str Pointeur vers les données caractères
     * @param length Nombre de caractères à copier depuis str
     * 
     * @note Permet de copier des sous-parties de buffer ou des données non-terminées par null.
     *       Ajoute automatiquement le null-terminator après la copie.
     *       Choix SSO/heap basé sur la longueur fournie.
     */
    NkString::NkString(const char* str, SizeType length)
        : mAllocator(&memory::NkGetDefaultAllocator())
        , mLength(0)
        , mCapacity(SSO_SIZE) {
        mSSOData[0] = '\0';
        SetSSO(true);
        if (str && length > 0) {
            if (length <= SSO_SIZE) {
                memory::NkMemCopy(mSSOData, str, length);
                mSSOData[length] = '\0';
                mLength = length;
            } else {
                MoveToHeap(length);
                memory::NkMemCopy(mHeapData, str, length);
                mHeapData[length] = '\0';
                mLength = length;
            }
        }
    }

    /**
     * @brief Constructeur depuis buffer + longueur avec allocateur personnalisé
     * 
     * @param str Pointeur vers les données caractères
     * @param length Nombre de caractères à copier
     * @param allocator Référence vers l'allocateur à utiliser
     * 
     * @note Délègue au constructeur avec allocateur, puis appelle Append(str, length).
     */
    NkString::NkString(const char* str, SizeType length, memory::NkIAllocator& allocator)
        : NkString(allocator) {
        if (str && length > 0) {
            Append(str, length);
        }
    }

    /**
     * @brief Constructeur depuis NkStringView
     * 
     * @param view Vue de chaîne à copier dans cette instance
     * 
     * @note Délègue au constructeur (data, length) de NkStringView.
     *       Crée une copie possédante : la vue source peut être détruite après.
     */
    NkString::NkString(NkStringView view)
        : NkString(view.Data(), view.Length()) {
    }

    /**
     * @brief Constructeur depuis NkStringView avec allocateur personnalisé
     * 
     * @param view Vue de chaîne à copier
     * @param allocator Référence vers l'allocateur à utiliser
     * 
     * @note Délègue au constructeur (data, length, allocator).
     */
    NkString::NkString(NkStringView view, memory::NkIAllocator& allocator)
        : NkString(view.Data(), view.Length(), allocator) {
    }

    /**
     * @brief Constructeur fill : répète un caractère N fois
     * 
     * @param count Nombre de répétitions du caractère
     * @param ch Caractère à répéter dans la chaîne résultante
     * 
     * @note Ex: NkString(5, 'x') crée "xxxxx"
     *       Utilise NkMemSet pour efficacité sur le remplissage.
     *       Choix SSO/heap basé sur count.
     */
    NkString::NkString(SizeType count, char ch)
        : mAllocator(&memory::NkGetDefaultAllocator())
        , mLength(0)
        , mCapacity(SSO_SIZE) {
        mSSOData[0] = '\0';
        SetSSO(true);
        if (count > 0) {
            if (count <= SSO_SIZE) {
                memory::NkMemSet(mSSOData, ch, count);
                mSSOData[count] = '\0';
                mLength = count;
            } else {
                MoveToHeap(count);
                memory::NkMemSet(mHeapData, ch, count);
                mHeapData[count] = '\0';
                mLength = count;
            }
        }
    }

    /**
     * @brief Constructeur fill avec allocateur personnalisé
     * 
     * @param count Nombre de répétitions
     * @param ch Caractère à répéter
     * @param allocator Référence vers l'allocateur à utiliser
     * 
     * @note Délègue au constructeur avec allocateur, puis appelle Append(count, ch).
     */
    NkString::NkString(SizeType count, char ch, memory::NkIAllocator& allocator)
        : NkString(allocator) {
        Append(count, ch);
    }

    /**
     * @brief Constructeur de copie
     * 
     * @param other Instance NkString à copier
     * 
     * @note Effectue une copie profonde des données.
     *       Préserve le mode SSO/heap de la source.
     *       Utilise le même allocateur que other.
     */
    NkString::NkString(const NkString& other)
        : mAllocator(other.mAllocator)
        , mLength(other.mLength)
        , mCapacity(other.mCapacity) {
        if (other.IsSSO()) {
            memory::NkMemCopy(mSSOData, other.mSSOData, mLength + 1);
            SetSSO(true);
        } else {
            AllocateHeap(mLength);
            memory::NkMemCopy(mHeapData, other.mHeapData, mLength + 1);
        }
    }

    /**
     * @brief Constructeur de copie avec allocateur personnalisé
     * 
     * @param other Instance NkString à copier
     * @param allocator Allocateur pour la nouvelle instance
     * 
     * @note Permet de copier une chaîne tout en changeant sa stratégie d'allocation.
     *       Délègue au constructeur avec allocateur, puis appelle Append(other).
     */
    NkString::NkString(const NkString& other, memory::NkIAllocator& allocator)
        : mAllocator(&allocator)
        , mLength(0)
        , mCapacity(SSO_SIZE) {
        mSSOData[0] = '\0';
        SetSSO(true);
        Append(other);
    }

    /**
     * @brief Constructeur de déplacement
     * 
     * @param other Instance NkString à déplacer
     * 
     * @note Transfère la propriété des ressources de other vers this.
     *       other devient une chaîne vide en mode SSO après l'opération.
     *       Complexité : O(1), aucune copie de données.
     *       Noexcept : garantit pas d'exception pour sécurité STL.
     */
    NkString::NkString(NkString&& other) noexcept
        : mAllocator(other.mAllocator)
        , mLength(other.mLength)
        , mCapacity(other.mCapacity) {
        if (other.IsSSO()) {
            memory::NkMemCopy(mSSOData, other.mSSOData, mLength + 1);
            SetSSO(true);
        } else {
            mHeapData = other.mHeapData;
            other.mHeapData = nullptr;
            SetSSO(false);
        }
        other.mLength = 0;
        other.mCapacity = SSO_SIZE;
        other.mSSOData[0] = '\0';
        other.SetSSO(true);
    }

    /**
     * @brief Destructeur
     * 
     * @note Libère les ressources heap si le mode heap est actif.
     *       Aucune action si SSO était utilisé (buffer stack).
     *       Garantit aucune fuite mémoire.
     */
    NkString::~NkString() {
        if (!IsSSO()) {
            DeallocateHeap();
        }
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
     *       Réalloue si capacité insuffisante via Clear() + Append().
     *       Préserve l'allocateur courant de this.
     */
    NkString& NkString::operator=(const NkString& other) {
        if (this != &other) {
            Clear();
            Append(other);
        }
        return *this;
    }

    /**
     * @brief Affectation par déplacement
     * 
     * @param other Instance source à déplacer
     * @return Référence vers this pour chaînage
     * 
     * @note Transfère les ressources de other vers this.
     *       other devient vide en mode SSO après l'opération.
     *       Gère correctement le cas heap/SSO des deux côtés.
     */
    NkString& NkString::operator=(NkString&& other) noexcept {
        if (this != &other) {
            if (!IsSSO()) {
                DeallocateHeap();
            }
            mLength = other.mLength;
            mCapacity = other.mCapacity;
            if (other.IsSSO()) {
                memory::NkMemCopy(mSSOData, other.mSSOData, mLength + 1);
                SetSSO(true);
            } else {
                mHeapData = other.mHeapData;
                other.mHeapData = nullptr;
                SetSSO(false);
            }
            other.mLength = 0;
            other.mCapacity = SSO_SIZE;
            other.mSSOData[0] = '\0';
            other.SetSSO(true);
        }
        return *this;
    }

    /**
     * @brief Affectation depuis une chaîne C-style
     * 
     * @param str Chaîne C terminée par null à copier
     * @return Référence vers this pour chaînage
     * 
     * @note Remplace le contenu actuel par une copie de str.
     *       Gère le cas str == nullptr (devient chaîne vide).
     */
    NkString& NkString::operator=(const char* str) {
        Clear();
        if (str) {
            Append(str);
        }
        return *this;
    }

    /**
     * @brief Affectation depuis NkStringView
     * 
     * @param view Vue de chaîne à copier
     * @return Référence vers this pour chaînage
     * 
     * @note Copie les données référencées par view.
     *       Plus efficace que conversion view -> NkString -> assign.
     */
    NkString& NkString::operator=(NkStringView view) {
        Clear();
        Append(view);
        return *this;
    }

    /**
     * @brief Affectation depuis un caractère unique
     * 
     * @param ch Caractère à assigner comme contenu unique
     * @return Référence vers this pour chaînage
     * 
     * @note Équivalent à Assign(1, ch).
     *       Utile pour réinitialisation rapide à un caractère.
     */
    NkString& NkString::operator=(char ch) {
        Clear();
        Append(ch);
        return *this;
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
    char& NkString::operator[](SizeType index) {
        NKENTSEU_ASSERT(index < mLength);
        return GetData()[index];
    }

    /**
     * @brief Accès indexé sans vérification de bornes (version constante)
     * 
     * @param index Position du caractère à accéder
     * @return Référence constante vers le caractère
     */
    const char& NkString::operator[](SizeType index) const {
        NKENTSEU_ASSERT(index < mLength);
        return GetData()[index];
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
    char& NkString::At(SizeType index) {
        NKENTSEU_ASSERT_MSG(index < mLength, "String index out of bounds");
        return GetData()[index];
    }

    /**
     * @brief Accès indexé avec vérification de bornes (version constante)
     * 
     * @param index Position du caractère à accéder
     * @return Référence constante vers le caractère
     */
    const char& NkString::At(SizeType index) const {
        NKENTSEU_ASSERT_MSG(index < mLength, "String index out of bounds");
        return GetData()[index];
    }

    /**
     * @brief Accès au premier caractère (version modifiable)
     * 
     * @return Référence modifiable vers le caractère en position 0
     * 
     * @warning Assertion si Empty() : vérifier avant appel.
     */
    char& NkString::Front() {
        NKENTSEU_ASSERT(mLength > 0);
        return GetData()[0];
    }

    /**
     * @brief Accès au premier caractère (version constante)
     * 
     * @return Référence constante vers le caractère en position 0
     */
    const char& NkString::Front() const {
        NKENTSEU_ASSERT(mLength > 0);
        return GetData()[0];
    }

    /**
     * @brief Accès au dernier caractère (version modifiable)
     * 
     * @return Référence modifiable vers le dernier caractère
     * 
     * @warning Assertion si Empty() : vérifier avant appel.
     */
    char& NkString::Back() {
        NKENTSEU_ASSERT(mLength > 0);
        return GetData()[mLength - 1];
    }

    /**
     * @brief Accès au dernier caractère (version constante)
     * 
     * @return Référence constante vers le dernier caractère
     */
    const char& NkString::Back() const {
        NKENTSEU_ASSERT(mLength > 0);
        return GetData()[mLength - 1];
    }

    /**
     * @brief Accès aux données brutes (version constante)
     * 
     * @return Pointeur constant vers le premier caractère
     * 
     * @note Retourne toujours un pointeur valide, même si Empty().
     *       Les données sont garanties null-terminated.
     */
    const char* NkString::Data() const noexcept {
        return GetData();
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
    char* NkString::Data() noexcept {
        return GetData();
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
    const char* NkString::CStr() const noexcept {
        return GetData();
    }

    // =====================================================================
    // SECTION : CAPACITÉ ET ÉTAT
    // =====================================================================

    /**
     * @brief Retourne le nombre de caractères dans la chaîne
     * 
     * @return Longueur actuelle en caractères (exclut null-terminator)
     * 
     * @note Complexité O(1) : valeur pré-calculée et maintenue.
     *       Ne pas confondre avec Capacity() qui inclut l'espace alloué.
     */
    NkString::SizeType NkString::Length() const noexcept {
        return mLength;
    }

    /**
     * @brief Alias pour Length() (convention STL)
     * 
     * @return Nombre d'éléments dans la chaîne
     */
    NkString::SizeType NkString::Size() const noexcept {
        return mLength;
    }

    /**
     * @brief Retourne la capacité totale allouée
     * 
     * @return Nombre maximal de caractères stockables sans réallocation
     * 
     * @note Capacity() >= Length() toujours.
     *       La différence représente l'espace de croissance réservé.
     */
    NkString::SizeType NkString::Capacity() const noexcept {
        return mCapacity;
    }

    /**
     * @brief Teste si la chaîne est vide
     * 
     * @return true si Length() == 0, false sinon
     * 
     * @note Plus efficace que Length() == 0 dans certains contextes.
     *       Ne dit rien sur la capacité allouée.
     */
    bool NkString::Empty() const noexcept {
        return mLength == 0;
    }

    /**
     * @brief Réserve de la capacité pour croissance future
     * 
     * @param newCapacity Nouvelle capacité minimale souhaitée
     * 
     * @note Si newCapacity <= Capacity(), aucune action.
     *       Si newCapacity > Capacity() et <= SSO_SIZE : no-op (déjà en SSO).
     *       Si newCapacity > SSO_SIZE : alloue heap et copie les données.
     *       Utile pour éviter les réallocations multiples en boucle.
     */
    void NkString::Reserve(SizeType newCapacity) {
        if (newCapacity <= mCapacity) {
            return;
        }
        if (IsSSO()) {
            if (newCapacity <= SSO_SIZE) {
                return;
            }
            MoveToHeap(newCapacity);
        } else {
            char* newData = static_cast<char*>(
                mAllocator->Allocate((newCapacity + 1) * sizeof(char)));
            memory::NkMemCopy(newData, mHeapData, mLength + 1);
            mAllocator->Deallocate(mHeapData);
            mHeapData = newData;
            mCapacity = newCapacity;
        }
    }

    /**
     * @brief Réduit la capacité au strict nécessaire
     * 
     * @note Si SSO actif : aucune action (déjà minimal).
     *       Si Length() <= SSO_SIZE : migre de heap vers SSO.
     *       Si Length() < Capacity() : réalloue heap à taille exacte.
     *       Peut déclencher une réallocation et copie.
     */
    void NkString::ShrinkToFit() {
        if (IsSSO()) {
            return;
        }
        if (mLength <= SSO_SIZE) {
            char temp[SSO_SIZE + 1];
            memory::NkMemCopy(temp, mHeapData, mLength + 1);
            DeallocateHeap();
            memory::NkMemCopy(mSSOData, temp, mLength + 1);
            mCapacity = SSO_SIZE;
            SetSSO(true);
        } else if (mLength < mCapacity) {
            char* newData = static_cast<char*>(
                mAllocator->Allocate((mLength + 1) * sizeof(char)));
            memory::NkMemCopy(newData, mHeapData, mLength + 1);
            mAllocator->Deallocate(mHeapData);
            mHeapData = newData;
            mCapacity = mLength;
        }
    }

    /**
     * @brief Vide la chaîne sans libérer la capacité
     * 
     * @note Met Length() à 0, préserve Capacity() pour réutilisation.
     *       Aucune libération mémoire : efficace pour réutilisation.
     *       Le null-terminator est maintenu en position 0.
     */
    void NkString::Clear() noexcept {
        mLength = 0;
        GetData()[0] = '\0';
    }

    // =====================================================================
    // SECTION : MODIFICATEURS - APPEND (AJOUT EN FIN)
    // =====================================================================

    /**
     * @brief Ajoute une chaîne C à la fin
     * 
     * @param str Chaîne C terminée par null à ajouter
     * @return Référence vers this pour chaînage
     * 
     * @note Délègue à Append(str, length) après calcul de length.
     *       Gère str == nullptr comme chaîne vide (no-op).
     */
    NkString& NkString::Append(const char* str) {
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
     *       Garantit null-termination après ajout.
     */
    NkString& NkString::Append(const char* str, SizeType length) {
        if (!str || length == 0) {
            return *this;
        }
        GrowIfNeeded(length);
        memory::NkMemCopy(GetData() + mLength, str, length);
        mLength += length;
        GetData()[mLength] = '\0';
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
    NkString& NkString::Append(const NkString& str) {
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
    NkString& NkString::Append(NkStringView view) {
        return Append(view.Data(), view.Length());
    }

    /**
     * @brief Ajoute N répétitions d'un caractère à la fin
     * 
     * @param count Nombre de fois répéter le caractère
     * @param ch Caractère à répéter
     * @return Référence vers this pour chaînage
     * 
     * @note Ex: str.Append(3, '-') ajoute "---" à la fin.
     *       Utilise NkMemSet pour efficacité sur remplissage.
     */
    NkString& NkString::Append(SizeType count, char ch) {
        if (count == 0) {
            return *this;
        }
        GrowIfNeeded(count);
        memory::NkMemSet(GetData() + mLength, ch, count);
        mLength += count;
        GetData()[mLength] = '\0';
        return *this;
    }

    /**
     * @brief Ajoute un caractère unique à la fin
     * 
     * @param ch Caractère à ajouter
     * @return Référence vers this pour chaînage
     * 
     * @note Équivalent à Append(1, ch).
     *       Méthode la plus courante pour construction caractère par caractère.
     */
    NkString& NkString::Append(char ch) {
        GrowIfNeeded(1);
        GetData()[mLength++] = ch;
        GetData()[mLength] = '\0';
        return *this;
    }

    /**
     * @brief Opérateur += pour chaîne C
     * 
     * @param str Chaîne C à concaténer
     * @return Référence vers this pour chaînage
     * 
     * @note Alias pour Append(str). Syntaxe plus concise.
     */
    NkString& NkString::operator+=(const char* str) {
        return Append(str);
    }

    /**
     * @brief Opérateur += pour NkString
     * 
     * @param str Instance NkString à concaténer
     * @return Référence vers this pour chaînage
     * 
     * @note Alias pour Append(str). Gère l'auto-référence.
     */
    NkString& NkString::operator+=(const NkString& str) {
        return Append(str);
    }

    /**
     * @brief Opérateur += pour NkStringView
     * 
     * @param view Vue de chaîne à concaténer
     * @return Référence vers this pour chaînage
     * 
     * @note Alias pour Append(view). Efficace sans copie temporaire.
     */
    NkString& NkString::operator+=(NkStringView view) {
        return Append(view);
    }

    /**
     * @brief Opérateur += pour caractère unique
     * 
     * @param ch Caractère à ajouter à la fin
     * @return Référence vers this pour chaînage
     * 
     * @note Alias pour Append(ch). Syntaxe intuitive.
     */
    NkString& NkString::operator+=(char ch) {
        return Append(ch);
    }

    // =====================================================================
    // SECTION : MODIFICATEURS - INSERT (INSERTION À POSITION)
    // =====================================================================

    /**
     * @brief Insère une chaîne C à une position donnée
     * 
     * @param pos Index où insérer les nouvelles données (0 = début)
     * @param str Chaîne C à insérer
     * @return Référence vers this pour chaînage
     * 
     * @note Délègue à Insert(pos, NkStringView(str)).
     *       Gère str == nullptr comme no-op.
     */
    NkString& NkString::Insert(SizeType pos, const char* str) {
        if (!str) {
            return *this;
        }
        return Insert(pos, NkStringView(str));
    }

    /**
     * @brief Insère une NkString à une position donnée
     * 
     * @param pos Index d'insertion
     * @param str Instance NkString à insérer
     * @return Référence vers this pour chaînage
     * 
     * @note Délègue à Insert(pos, str.View()).
     */
    NkString& NkString::Insert(SizeType pos, const NkString& str) {
        return Insert(pos, str.View());
    }

    /**
     * @brief Insère une NkStringView à une position donnée
     * 
     * @param pos Index d'insertion
     * @param view Vue de chaîne à insérer
     * @return Référence vers this pour chaînage
     * 
     * @note Décale les caractères existants vers la droite via NkMemMove.
     *       Assertion si pos > Length().
     *       Garantit null-termination après insertion.
     */
    NkString& NkString::Insert(SizeType pos, NkStringView view) {
        NKENTSEU_ASSERT(pos <= mLength);
        if (view.Empty()) {
            return *this;
        }
        GrowIfNeeded(view.Length());
        char* data = GetData();
        memory::NkMemMove(data + pos + view.Length(), data + pos, mLength - pos);
        memory::NkMemCopy(data + pos, view.Data(), view.Length());
        mLength += view.Length();
        data[mLength] = '\0';
        return *this;
    }

    /**
     * @brief Insère N répétitions d'un caractère à une position
     * 
     * @param pos Index d'insertion
     * @param count Nombre de répétitions du caractère
     * @param ch Caractère à insérer
     * @return Référence vers this pour chaînage
     * 
     * @note Ex: str.Insert(2, 3, '*') sur "abcd" donne "ab***cd"
     *       Utilise NkMemSet pour efficacité sur remplissage.
     */
    NkString& NkString::Insert(SizeType pos, SizeType count, char ch) {
        NKENTSEU_ASSERT(pos <= mLength);
        if (count == 0) {
            return *this;
        }
        GrowIfNeeded(count);
        char* data = GetData();
        memory::NkMemMove(data + pos + count, data + pos, mLength - pos);
        memory::NkMemSet(data + pos, ch, count);
        mLength += count;
        data[mLength] = '\0';
        return *this;
    }

    // =====================================================================
    // SECTION : MODIFICATEURS - ERASE (SUPPRESSION)
    // =====================================================================

    /**
     * @brief Supprime une plage de caractères
     * 
     * @param pos Index de début de la plage à supprimer (défaut: 0)
     * @param count Nombre de caractères à supprimer (défaut: npos = jusqu'à la fin)
     * @return Référence vers this pour chaînage
     * 
     * @note Décale les caractères suivants vers la gauche via NkMemMove.
     *       Ne réduit pas automatiquement la capacité : utiliser ShrinkToFit() si nécessaire.
     *       Garantit null-termination après suppression.
     */
    NkString& NkString::Erase(SizeType pos, SizeType count) {
        NKENTSEU_ASSERT(pos <= mLength);
        if (pos == mLength) {
            return *this;
        }
        if (count == npos || pos + count >= mLength) {
            count = mLength - pos;
        }
        char* data = GetData();
        memory::NkMemMove(data + pos, data + pos + count, mLength - pos - count + 1);
        mLength -= count;
        return *this;
    }

    /**
     * @brief Ajoute un caractère à la fin (alias Push)
     * 
     * @param ch Caractère à ajouter
     * 
     * @note Équivalent à Append(ch). Nom convention STL.
     */
    void NkString::PushBack(char ch) {
        Append(ch);
    }

    /**
     * @brief Supprime le dernier caractère
     * 
     * @note Assertion si Empty().
     *       Ne réduit pas la capacité : efficient pour pop/push répétés.
     *       Met à jour le null-terminator.
     */
    void NkString::PopBack() {
        NKENTSEU_ASSERT(mLength > 0);
        --mLength;
        GetData()[mLength] = '\0';
    }

    // =====================================================================
    // SECTION : MODIFICATEURS - REPLACE (REMPLACEMENT)
    // =====================================================================

    /**
     * @brief Remplace une plage par une chaîne C
     * 
     * @param pos Index de début de la plage à remplacer
     * @param count Nombre de caractères à remplacer
     * @param str Chaîne C de remplacement
     * @return Référence vers this pour chaînage
     * 
     * @note Délègue à Replace(pos, count, NkStringView(str)).
     */
    NkString& NkString::Replace(SizeType pos, SizeType count, const char* str) {
        return Replace(pos, count, NkStringView(str));
    }

    /**
     * @brief Remplace une plage par une NkString
     * 
     * @param pos Index de début de la plage
     * @param count Nombre de caractères à remplacer
     * @param str Instance NkString de remplacement
     * @return Référence vers this pour chaînage
     * 
     * @note Délègue à Replace(pos, count, str.View()).
     */
    NkString& NkString::Replace(SizeType pos, SizeType count, const NkString& str) {
        return Replace(pos, count, str.View());
    }

    /**
     * @brief Remplace une plage par une NkStringView
     * 
     * @param pos Index de début de la plage
     * @param count Nombre de caractères à remplacer
     * @param view Vue de chaîne de remplacement
     * @return Référence vers this pour chaînage
     * 
     * @note Si count == view.Length() : copie directe in-place.
     *       Sinon : Erase + Insert pour gérer différence de taille.
     *       Plus efficace car optimisé en une seule passe quand possible.
     */
    NkString& NkString::Replace(SizeType pos, SizeType count, NkStringView view) {
        NKENTSEU_ASSERT(pos <= mLength);
        if (pos + count > mLength) {
            count = mLength - pos;
        }
        if (count == view.Length()) {
            memory::NkMemCopy(GetData() + pos, view.Data(), view.Length());
        } else {
            Erase(pos, count);
            Insert(pos, view);
        }
        return *this;
    }

    // =====================================================================
    // SECTION : MODIFICATEURS - RESIZE (REDIMENSIONNEMENT)
    // =====================================================================

    /**
     * @brief Redimensionne la chaîne à une longueur cible
     * 
     * @param count Nouvelle longueur souhaitée
     * 
     * @note Délègue à Resize(count, '\\0').
     *       Si count < Length() : tronque à count caractères.
     *       Si count > Length() : étend avec caractères null.
     */
    void NkString::Resize(SizeType count) {
        Resize(count, '\0');
    }

    /**
     * @brief Redimensionne avec caractère de remplissage personnalisé
     * 
     * @param count Nouvelle longueur souhaitée
     * @param ch Caractère à utiliser pour l'extension
     * 
     * @note Si count < Length() : tronque à count caractères.
     *       Si count > Length() : étend avec ch au lieu de '\\0'.
     *       Peut déclencher réallocation si count > Capacity().
     */
    void NkString::Resize(SizeType count, char ch) {
        if (count < mLength) {
            mLength = count;
            GetData()[mLength] = '\0';
        } else if (count > mLength) {
            GrowIfNeeded(count - mLength);
            memory::NkMemSet(GetData() + mLength, ch, count - mLength);
            mLength = count;
            GetData()[mLength] = '\0';
        }
    }

    /**
     * @brief Échange le contenu avec une autre instance en O(1)
     * 
     * @param other Référence vers l'autre NkString à échanger
     * 
     * @note Utilise un temporaire et les move constructors/assignements.
     *       Échange uniquement les pointeurs et métadonnées.
     *       Aucune copie de données : très efficace.
     *       Noexcept : garantie pour conteneurs STL-like.
     */
    void NkString::Swap(NkString& other) noexcept {
        NkString temp(static_cast<NkString&&>(*this));
        *this = static_cast<NkString&&>(other);
        other = static_cast<NkString&&>(temp);
    }

    // =========================================================================
    // SECTION : MÉTHODES PRIVÉES (SSO / HEAP)
    // =========================================================================

    bool NkString::IsSSO() const noexcept {
        return mCapacity == SSO_SIZE;
    }

    void NkString::SetSSO(bool sso) noexcept {
        if (sso) {
            mCapacity = SSO_SIZE;
        }
    }

    char* NkString::GetData() noexcept {
        return IsSSO() ? mSSOData : mHeapData;
    }

    const char* NkString::GetData() const noexcept {
        return IsSSO() ? mSSOData : mHeapData;
    }

    void NkString::AllocateHeap(SizeType capacity) {
        mHeapData = static_cast<char*>(mAllocator->Allocate((capacity + 1) * sizeof(char)));
        mCapacity = capacity;
    }

    void NkString::DeallocateHeap() {
        if (!IsSSO() && mHeapData) {
            mAllocator->Deallocate(mHeapData);
            mHeapData = nullptr;
        }
    }

    void NkString::MoveToHeap(SizeType newCapacity) {
        char temp[SSO_SIZE + 1];
        memory::NkMemCopy(temp, mSSOData, mLength + 1);
        AllocateHeap(newCapacity);
        memory::NkMemCopy(mHeapData, temp, mLength + 1);
    }

    void NkString::GrowIfNeeded(SizeType additionalSize) {
        SizeType needed = mLength + additionalSize;
        if (needed <= mCapacity) return;
        SizeType newCapacity = CalculateGrowth(mCapacity, needed);
        if (IsSSO()) {
            MoveToHeap(newCapacity);
        } else {
            Reserve(newCapacity);
        }
    }

    NkString::SizeType NkString::CalculateGrowth(SizeType current, SizeType needed) {
        SizeType growth = current + (current / 2);
        return growth > needed ? growth : needed;
    }

    // =========================================================================
    // SECTION : CONVERSION
    // =========================================================================

    NkStringView NkString::View() const noexcept {
        return NkStringView(GetData(), mLength);
    }

    NkString::operator NkStringView() const noexcept {
        return View();
    }

    // =========================================================================
    // SECTION : ITÉRATEURS
    // =========================================================================

    const char* NkString::begin() const noexcept {
        return GetData();
    }

    const char* NkString::end() const noexcept {
        return GetData() + mLength;
    }

    // =========================================================================
    // SECTION : OPÉRATIONS CHAÎNE
    // =========================================================================

    NkString NkString::SubStr(SizeType pos, SizeType count) const {
        if (pos > mLength) pos = mLength;
        if (count == npos || pos + count > mLength) count = mLength - pos;
        return NkString(GetData() + pos, count);
    }

    int32 NkString::Compare(const NkString& other) const noexcept {
        return Compare(other.View());
    }

    int32 NkString::Compare(NkStringView other) const noexcept {
        SizeType minLen = mLength < other.Length() ? mLength : other.Length();
        int result = memory::NkMemCompare(GetData(), other.Data(), minLen);
        if (result != 0) return result;
        if (mLength < other.Length()) return -1;
        if (mLength > other.Length()) return 1;
        return 0;
    }

    int32 NkString::Compare(const char* str) const noexcept {
        return Compare(NkStringView(str));
    }

    bool NkString::StartsWith(NkStringView prefix) const noexcept {
        if (prefix.Length() > mLength) return false;
        return memory::NkMemCompare(GetData(), prefix.Data(), prefix.Length()) == 0;
    }

    bool NkString::StartsWith(char ch) const noexcept {
        return mLength > 0 && GetData()[0] == ch;
    }

    bool NkString::EndsWith(NkStringView suffix) const noexcept {
        if (suffix.Length() > mLength) return false;
        return memory::NkMemCompare(GetData() + mLength - suffix.Length(), suffix.Data(), suffix.Length()) == 0;
    }

    bool NkString::EndsWith(char ch) const noexcept {
        return mLength > 0 && GetData()[mLength - 1] == ch;
    }

    bool NkString::Contains(NkStringView str) const noexcept {
        return Find(str) != npos;
    }

    bool NkString::Contains(char ch) const noexcept {
        return Find(ch) != npos;
    }

    NkString::SizeType NkString::Find(NkStringView str, SizeType pos) const noexcept {
        return View().Find(str, pos);
    }

    NkString::SizeType NkString::Find(char ch, SizeType pos) const noexcept {
        return View().Find(ch, pos);
    }

    NkString::SizeType NkString::RFind(NkStringView str, SizeType pos) const noexcept {
        if (str.Empty()) return mLength;
        if (str.Length() > mLength) return npos;
        SizeType searchPos = (pos == npos || pos > mLength - str.Length()) ? mLength - str.Length() : pos;
        for (SizeType i = searchPos + 1; i > 0; --i) {
            if (memory::NkMemCompare(GetData() + i - 1, str.Data(), str.Length()) == 0)
                return i - 1;
        }
        return npos;
    }

    NkString::SizeType NkString::RFind(char ch, SizeType pos) const noexcept {
        return View().RFind(ch, pos);
    }

    NkString::SizeType NkString::FindFirstOf(NkStringView chars, SizeType pos) const noexcept {
        if (pos >= mLength) return npos;
        const char* data = GetData();
        for (SizeType i = pos; i < mLength; ++i) {
            for (SizeType j = 0; j < chars.Length(); ++j) {
                if (data[i] == chars[j]) return i;
            }
        }
        return npos;
    }

    NkString::SizeType NkString::FindLastOf(NkStringView chars, SizeType pos) const noexcept {
        if (mLength == 0) return npos;
        SizeType searchPos = (pos == npos || pos >= mLength) ? mLength - 1 : pos;
        const char* data = GetData();
        for (SizeType i = searchPos + 1; i > 0; --i) {
            SizeType idx = i - 1;
            for (SizeType j = 0; j < chars.Length(); ++j) {
                if (data[idx] == chars[j]) return idx;
            }
        }
        return npos;
    }

    NkString::SizeType NkString::FindFirstNotOf(NkStringView chars, SizeType pos) const noexcept {
        if (pos >= mLength) return npos;
        const char* data = GetData();
        for (SizeType i = pos; i < mLength; ++i) {
            bool found = false;
            for (SizeType j = 0; j < chars.Length(); ++j) {
                if (data[i] == chars[j]) { found = true; break; }
            }
            if (!found) return i;
        }
        return npos;
    }

    NkString::SizeType NkString::FindLastNotOf(NkStringView chars, SizeType pos) const noexcept {
        if (mLength == 0) return npos;
        SizeType searchPos = (pos == npos || pos >= mLength) ? mLength - 1 : pos;
        const char* data = GetData();
        for (SizeType i = searchPos + 1; i > 0; --i) {
            SizeType idx = i - 1;
            bool found = false;
            for (SizeType j = 0; j < chars.Length(); ++j) {
                if (data[idx] == chars[j]) { found = true; break; }
            }
            if (!found) return idx;
        }
        return npos;
    }

    // =========================================================================
    // SECTION : TRANSFORMATIONS
    // =========================================================================

    NkString& NkString::ToLower() {
        char* data = GetData();
        for (SizeType i = 0; i < mLength; ++i)
            data[i] = string::NkToLower(data[i]);
        return *this;
    }

    NkString& NkString::ToUpper() {
        char* data = GetData();
        for (SizeType i = 0; i < mLength; ++i)
            data[i] = string::NkToUpper(data[i]);
        return *this;
    }

    NkString& NkString::TrimLeft() {
        const char* data = GetData();
        SizeType start = 0;
        while (start < mLength && string::NkIsWhitespace(data[start])) ++start;
        if (start > 0) Erase(0, start);
        return *this;
    }

    NkString& NkString::TrimRight() {
        if (mLength == 0) return *this;
        char* data = GetData();
        SizeType end = mLength;
        while (end > 0 && string::NkIsWhitespace(data[end - 1])) --end;
        if (end < mLength) { mLength = end; data[mLength] = '\0'; }
        return *this;
    }

    NkString& NkString::Trim() {
        TrimRight();
        TrimLeft();
        return *this;
    }

    NkString& NkString::Reverse() {
        char* data = GetData();
        for (SizeType i = 0; i < mLength / 2; ++i) {
            char tmp = data[i];
            data[i] = data[mLength - i - 1];
            data[mLength - i - 1] = tmp;
        }
        return *this;
    }

    NkString& NkString::Capitalize() {
        if (mLength == 0) return *this;
        char* data = GetData();
        data[0] = string::NkToUpper(data[0]);
        for (SizeType i = 1; i < mLength; ++i)
            data[i] = string::NkToLower(data[i]);
        return *this;
    }

    NkString& NkString::TitleCase() {
        if (mLength == 0) return *this;
        char* data = GetData();
        bool newWord = true;
        for (SizeType i = 0; i < mLength; ++i) {
            if (string::NkIsWhitespace(data[i]) || data[i] == '-') { newWord = true; }
            else if (newWord) { data[i] = string::NkToUpper(data[i]); newWord = false; }
            else { data[i] = string::NkToLower(data[i]); }
        }
        return *this;
    }

    NkString& NkString::RemoveChars(NkStringView charsToRemove) {
        if (charsToRemove.Empty()) return *this;
        char* data = GetData();
        SizeType writePos = 0;
        for (SizeType r = 0; r < mLength; ++r) {
            bool remove = false;
            for (SizeType j = 0; j < charsToRemove.Length(); ++j) {
                if (data[r] == charsToRemove[j]) { remove = true; break; }
            }
            if (!remove) data[writePos++] = data[r];
        }
        mLength = writePos;
        data[mLength] = '\0';
        return *this;
    }

    NkString& NkString::RemoveAll(char ch) {
        char* data = GetData();
        SizeType writePos = 0;
        for (SizeType r = 0; r < mLength; ++r) {
            if (data[r] != ch) data[writePos++] = data[r];
        }
        mLength = writePos;
        data[mLength] = '\0';
        return *this;
    }

    // =========================================================================
    // SECTION : CONVERSIONS NUMÉRIQUES
    // =========================================================================

    bool NkString::ToInt(int32& out) const noexcept {
        return string::NkParseInt(View(), out);
    }

    bool NkString::ToFloat(float32& out) const noexcept {
        return string::NkParseFloat(View(), out);
    }

    bool NkString::ToInt64(int64& out) const noexcept {
        return string::NkParseInt64(View(), out);
    }

    bool NkString::ToUInt(uint32& out) const noexcept {
        return string::NkParseUInt(View(), out);
    }

    bool NkString::ToUInt64(uint64& out) const noexcept {
        return string::NkParseUInt64(View(), out);
    }

    bool NkString::ToDouble(float64& out) const noexcept {
        return string::NkParseDouble(View(), out);
    }

    bool NkString::ToBool(bool& out) const noexcept {
        return string::NkParseBool(View(), out);
    }

    int32 NkString::ToInt32(int32 defaultValue) const noexcept {
        int32 result;
        return ToInt(result) ? result : defaultValue;
    }

    float32 NkString::ToFloat32(float32 defaultValue) const noexcept {
        float32 result;
        return ToFloat(result) ? result : defaultValue;
    }

    bool NkString::IsDigits() const noexcept     { return string::NkIsAllDigits(View()); }
    bool NkString::IsAlpha() const noexcept      { return string::NkIsAllAlpha(View()); }
    bool NkString::IsAlphaNumeric() const noexcept { return string::NkIsAllAlphaNumeric(View()); }
    bool NkString::IsWhitespace() const noexcept { return string::NkIsAllWhitespace(View()); }
    bool NkString::IsNumeric() const noexcept    { return string::NkIsNumeric(View()); }
    bool NkString::IsInteger() const noexcept    { return string::NkIsInteger(View()); }

    // =========================================================================
    // SECTION : HASH
    // =========================================================================

    uint64 NkString::Hash() const noexcept {
        return string::NkHashFNV1a64(View());
    }

    // =========================================================================
    // SECTION : FORMATAGE PRINTF-STYLE
    // =========================================================================

    NkString NkString::VFormat(const char* format, va_list args) {
        NkString result;
        if (!format) return result;
        va_list args_copy;
        va_copy(args_copy, args);
        int size = std::vsnprintf(nullptr, 0, format, args_copy);
        va_end(args_copy);
        if (size <= 0) return result;
        result.Reserve(static_cast<SizeType>(size));
        std::vsnprintf(result.GetData(), static_cast<size_t>(size) + 1, format, args);
        result.mLength = static_cast<SizeType>(size);
        return result;
    }

    NkString NkString::Format(const char* format, ...) {
        va_list args;
        va_start(args, format);
        NkString result = VFormat(format, args);
        va_end(args);
        return result;
    }

    NkString NkString::VFmtf(const char* format, va_list args) {
        return VFormat(format, args);
    }

    NkString NkString::Fmtf(const char* format, ...) {
        va_list args;
        va_start(args, format);
        NkString result = VFormat(format, args);
        va_end(args);
        return result;
    }

    // =========================================================================
    // SECTION : OPÉRATEURS NON-MEMBRES
    // =========================================================================

    NkString operator+(const NkString& lhs, const NkString& rhs) {
        NkString result(lhs);
        result.Append(rhs);
        return result;
    }

    NkString operator+(const NkString& lhs, const char* rhs) {
        NkString result(lhs);
        result.Append(rhs);
        return result;
    }

    NkString operator+(const char* lhs, const NkString& rhs) {
        NkString result(lhs);
        result.Append(rhs);
        return result;
    }

    NkString operator+(const NkString& lhs, char rhs) {
        NkString result(lhs);
        result.Append(rhs);
        return result;
    }

    NkString operator+(char lhs, const NkString& rhs) {
        NkString result(1, lhs);
        result.Append(rhs);
        return result;
    }

    bool operator==(const NkString& lhs, const NkString& rhs) noexcept {
        return lhs.Compare(rhs) == 0;
    }

    bool operator==(const NkString& lhs, const char* rhs) noexcept {
        return lhs.Compare(rhs) == 0;
    }

    bool operator!=(const NkString& lhs, const NkString& rhs) noexcept {
        return lhs.Compare(rhs) != 0;
    }

    bool operator!=(const NkString& lhs, const char* rhs) noexcept {
        return lhs.Compare(rhs) != 0;
    }

    bool operator<(const NkString& lhs, const NkString& rhs) noexcept {
        return lhs.Compare(rhs) < 0;
    }

    bool operator<=(const NkString& lhs, const NkString& rhs) noexcept {
        return lhs.Compare(rhs) <= 0;
    }

    bool operator>(const NkString& lhs, const NkString& rhs) noexcept {
        return lhs.Compare(rhs) > 0;
    }

    bool operator>=(const NkString& lhs, const NkString& rhs) noexcept {
        return lhs.Compare(rhs) >= 0;
    }

    // =========================================================================
    // SECTION : ALLOCATEUR
    // =========================================================================

    memory::NkIAllocator& NkString::GetAllocator() const noexcept {
        return *mAllocator;
    }

} // namespace nkentseu

// -----------------------------------------------------------------------------
// EXEMPLES D'UTILISATION - PARTIE 1
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
    }

    // =====================================================================
    // Exemple 2 : Gestion d'allocateur personnalisé pour pooling
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
    // Exemple 3 : Optimisation avec Reserve avant boucle d'ajouts
    // =====================================================================
    {
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
    }
*/