// =============================================================================
// NKSerialization/NkArchive.cpp
// Implémentation de l'archive centrale clé/valeur NkArchive.
//
// Design :
//  - Implémentation minimale : logique métier dans le header pour inlining
//  - Gestion manuelle de la mémoire pour NkArchiveNode::object (owning raw ptr)
//  - Coercition de types dans les getters avec fallback parsing depuis string
//  - Navigation hiérarchique récursive via SetPath/GetPath
//  - Méta-données stockées sous clé "__meta__" comme objet imbriqué
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#include "NKSerialization/NkArchive.h"

// -------------------------------------------------------------------------
// EN-TÊTES STANDARDS POUR FORMATAGE ET UTILITAIRES
// -------------------------------------------------------------------------
// Inclusions minimales pour les opérations de formatage numérique.
// Utilisation de NkString::Fmtf pour cohérence avec le reste du projet.

#include <cstdio>
#include <cstdlib>


namespace nkentseu {


    // =============================================================================
    // NkArchiveValue — IMPLÉMENTATION DES FACTORIES
    // =============================================================================
    // Ces méthodes garantissent la cohérence entre type, raw et text.
    // Formatage canonique pour assurer la réversibilité des conversions.

    // -------------------------------------------------------------------------
    // MÉTHODE : Null
    // DESCRIPTION : Crée une valeur null (absence de donnée)
    // -------------------------------------------------------------------------
    NkArchiveValue NkArchiveValue::Null() noexcept {
        NkArchiveValue v;
        v.type = NkArchiveValueType::NK_VALUE_NULL;
        // text reste vide par défaut : représentation canonique de null
        return v;
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : FromBool
    // DESCRIPTION : Crée une valeur booléenne avec texte "true"/"false"
    // -------------------------------------------------------------------------
    NkArchiveValue NkArchiveValue::FromBool(nk_bool b) noexcept {
        NkArchiveValue v;
        v.type = NkArchiveValueType::NK_VALUE_BOOL;
        v.raw.b = b;
        v.text = b ? NkString("true") : NkString("false");
        return v;
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : FromInt32
    // DESCRIPTION : Wrapper vers FromInt64 pour unicité du type interne
    // -------------------------------------------------------------------------
    NkArchiveValue NkArchiveValue::FromInt32(nk_int32 i) noexcept {
        return FromInt64(static_cast<nk_int64>(i));
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : FromInt64
    // DESCRIPTION : Crée un entier signé 64 bits avec formatage décimal
    // -------------------------------------------------------------------------
    NkArchiveValue NkArchiveValue::FromInt64(nk_int64 i) noexcept {
        NkArchiveValue v;
        v.type = NkArchiveValueType::NK_VALUE_INT64;
        v.raw.i = i;
        v.text = NkString::Fmtf("%lld", static_cast<long long>(i));
        return v;
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : FromUInt32
    // DESCRIPTION : Wrapper vers FromUInt64 pour unicité du type interne
    // -------------------------------------------------------------------------
    NkArchiveValue NkArchiveValue::FromUInt32(nk_uint32 u) noexcept {
        return FromUInt64(static_cast<nk_uint64>(u));
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : FromUInt64
    // DESCRIPTION : Crée un entier non-signé 64 bits avec formatage décimal
    // -------------------------------------------------------------------------
    NkArchiveValue NkArchiveValue::FromUInt64(nk_uint64 u) noexcept {
        NkArchiveValue v;
        v.type = NkArchiveValueType::NK_VALUE_UINT64;
        v.raw.u = u;
        v.text = NkString::Fmtf("%llu", static_cast<unsigned long long>(u));
        return v;
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : FromFloat32
    // DESCRIPTION : Wrapper vers FromFloat64 pour précision maximale
    // -------------------------------------------------------------------------
    NkArchiveValue NkArchiveValue::FromFloat32(nk_float32 f) noexcept {
        return FromFloat64(static_cast<nk_float64>(f));
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : FromFloat64
    // DESCRIPTION : Crée un flottant 64 bits avec formatage "%.17g" réversible
    // -------------------------------------------------------------------------
    NkArchiveValue NkArchiveValue::FromFloat64(nk_float64 f) noexcept {
        NkArchiveValue v;
        v.type = NkArchiveValueType::NK_VALUE_FLOAT64;
        v.raw.f = f;
        // "%.17g" garantit la réversibilité : float→string→float sans perte
        v.text = NkString::Fmtf("%.17g", f);
        return v;
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : FromString
    // DESCRIPTION : Crée une valeur string avec copie du contenu
    // -------------------------------------------------------------------------
    NkArchiveValue NkArchiveValue::FromString(NkStringView sv) noexcept {
        NkArchiveValue v;
        v.type = NkArchiveValueType::NK_VALUE_STRING;
        v.text = NkString(sv);
        return v;
    }


    // =============================================================================
    // NkArchiveNode — IMPLÉMENTATION DU BIG FIVE
    // =============================================================================
    // Gestion manuelle de la mémoire pour le variant object (owning raw pointer).
    // CloneNode() est une fonction libre statique pour éviter la duplication de code.

    // -------------------------------------------------------------------------
    // FONCTION LIBRE : CloneNode
    // DESCRIPTION : Helper interne pour copie profonde d'un NkArchiveNode
    // -------------------------------------------------------------------------
    static void CloneNode(NkArchiveNode& dst, const NkArchiveNode& src) noexcept {
        dst.kind = src.kind;
        dst.value = src.value;

        if (src.kind == NkNodeKind::NK_NODE_OBJECT && src.object) {
            // Copie profonde de l'objet imbriqué
            dst.object = new NkArchive(*src.object);
        } else {
            dst.object = nullptr;
        }

        if (src.kind == NkNodeKind::NK_NODE_ARRAY) {
            // Copie sémantique du tableau via NkVector::operator=
            dst.array = src.array;
        }
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : FreeObject
    // DESCRIPTION : Libère l'objet possédé si présent
    // -------------------------------------------------------------------------
    void NkArchiveNode::FreeObject() noexcept {
        delete object;
        object = nullptr;
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : Destructeur
    // DESCRIPTION : Cleanup automatique des ressources possédées
    // -------------------------------------------------------------------------
    NkArchiveNode::~NkArchiveNode() noexcept {
        FreeObject();
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : Constructeur de copie
    // DESCRIPTION : Duplication profonde via CloneNode helper
    // -------------------------------------------------------------------------
    NkArchiveNode::NkArchiveNode(const NkArchiveNode& o) noexcept {
        CloneNode(*this, o);
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : Constructeur de move
    // DESCRIPTION : Transfert de propriété avec réinitialisation de la source
    // -------------------------------------------------------------------------
    NkArchiveNode::NkArchiveNode(NkArchiveNode&& o) noexcept
        : kind(o.kind)
        , value(std::move(o.value))
        , object(o.object)
        , array(std::move(o.array))
    {
        // Réinitialisation de la source pour sécurité post-move
        o.object = nullptr;
        o.kind = NkNodeKind::NK_NODE_SCALAR;
        // array est déjà vidé par std::move, value est dans état valide
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : Opérateur d'affectation par copie
    // DESCRIPTION : Copie profonde avec auto-assignation guard et cleanup
    // -------------------------------------------------------------------------
    NkArchiveNode& NkArchiveNode::operator=(const NkArchiveNode& o) noexcept {
        if (this != &o) {
            FreeObject();
            array.Clear();
            CloneNode(*this, o);
        }
        return *this;
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : Opérateur d'affectation par move
    // DESCRIPTION : Transfert de propriété avec auto-assignation guard
    // -------------------------------------------------------------------------
    NkArchiveNode& NkArchiveNode::operator=(NkArchiveNode&& o) noexcept {
        if (this != &o) {
            FreeObject();
            array.Clear();
            kind = o.kind;
            value = std::move(o.value);
            object = o.object;
            array = std::move(o.array);
            // Réinitialisation de la source
            o.object = nullptr;
            o.kind = NkNodeKind::NK_NODE_SCALAR;
        }
        return *this;
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : SetObject
    // DESCRIPTION : Configure ce nœud comme objet avec copie profonde
    // -------------------------------------------------------------------------
    void NkArchiveNode::SetObject(const NkArchive& arc) noexcept {
        FreeObject();
        array.Clear();
        kind = NkNodeKind::NK_NODE_OBJECT;
        object = new NkArchive(arc);
    }


    // =============================================================================
    // NkArchive — HELPERS INTERNES
    // =============================================================================

    // -------------------------------------------------------------------------
    // MÉTHODE : FindIndex
    // DESCRIPTION : Recherche linéaire d'une clé dans mEntries
    // -------------------------------------------------------------------------
    nk_size NkArchive::FindIndex(NkStringView key) const noexcept {
        for (nk_size i = 0; i < mEntries.Size(); ++i) {
            if (mEntries[i].key.Compare(key) == 0) {
                return i;
            }
        }
        return NPOS;
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : IsValidKey
    // DESCRIPTION : Validation basique d'une clé (non-vide)
    // -------------------------------------------------------------------------
    bool NkArchive::IsValidKey(NkStringView key) noexcept {
        return !key.Empty();
    }


    // =============================================================================
    // NkArchive — ACCÈS AUX NŒUDS BRUTS
    // =============================================================================

    // -------------------------------------------------------------------------
    // MÉTHODE : SetNode
    // DESCRIPTION : Insertion ou mise à jour d'un nœud par clé
    // -------------------------------------------------------------------------
    nk_bool NkArchive::SetNode(NkStringView key, const NkArchiveNode& node) noexcept {
        if (!IsValidKey(key)) {
            return false;
        }

        nk_size idx = FindIndex(key);

        if (idx != NPOS) {
            // Mise à jour de l'entrée existante
            mEntries[idx].node = node;
            return true;
        }

        // Création d'une nouvelle entrée
        NkArchiveEntry e;
        e.key = NkString(key);
        e.node = node;
        mEntries.PushBack(std::move(e));
        return true;
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : GetNode
    // DESCRIPTION : Récupération par copie d'un nœud existant
    // -------------------------------------------------------------------------
    nk_bool NkArchive::GetNode(NkStringView key, NkArchiveNode& out) const noexcept {
        nk_size idx = FindIndex(key);
        if (idx == NPOS) {
            return false;
        }
        out = mEntries[idx].node;
        return true;
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : FindNode (const)
    // DESCRIPTION : Accès en lecture par pointeur sans copie
    // -------------------------------------------------------------------------
    const NkArchiveNode* NkArchive::FindNode(NkStringView key) const noexcept {
        nk_size idx = FindIndex(key);
        return (idx != NPOS) ? &mEntries[idx].node : nullptr;
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : FindNode (mutable)
    // DESCRIPTION : Accès en écriture par pointeur sans copie
    // -------------------------------------------------------------------------
    NkArchiveNode* NkArchive::FindNode(NkStringView key) noexcept {
        nk_size idx = FindIndex(key);
        return (idx != NPOS) ? &mEntries[idx].node : nullptr;
    }


    // =============================================================================
    // NkArchive — SETTERS/GETTERS SCALAIRES
    // =============================================================================

    // -------------------------------------------------------------------------
    // MÉTHODE : SetValue
    // DESCRIPTION : Wrapper vers SetNode avec encapsulation scalaire
    // -------------------------------------------------------------------------
    nk_bool NkArchive::SetValue(NkStringView key, const NkArchiveValue& v) noexcept {
        return SetNode(key, NkArchiveNode(v));
    }

    // -------------------------------------------------------------------------
    // MÉTHODES : Set* scalaires
    // DESCRIPTION : Wrappers type-safe vers SetValue avec factory appropriée
    // -------------------------------------------------------------------------
    nk_bool NkArchive::SetNull(NkStringView k) noexcept {
        return SetValue(k, NkArchiveValue::Null());
    }

    nk_bool NkArchive::SetBool(NkStringView k, nk_bool b) noexcept {
        return SetValue(k, NkArchiveValue::FromBool(b));
    }

    nk_bool NkArchive::SetInt32(NkStringView k, nk_int32 i) noexcept {
        return SetValue(k, NkArchiveValue::FromInt32(i));
    }

    nk_bool NkArchive::SetInt64(NkStringView k, nk_int64 i) noexcept {
        return SetValue(k, NkArchiveValue::FromInt64(i));
    }

    nk_bool NkArchive::SetUInt32(NkStringView k, nk_uint32 u) noexcept {
        return SetValue(k, NkArchiveValue::FromUInt32(u));
    }

    nk_bool NkArchive::SetUInt64(NkStringView k, nk_uint64 u) noexcept {
        return SetValue(k, NkArchiveValue::FromUInt64(u));
    }

    nk_bool NkArchive::SetFloat32(NkStringView k, nk_float32 f) noexcept {
        return SetValue(k, NkArchiveValue::FromFloat32(f));
    }

    nk_bool NkArchive::SetFloat64(NkStringView k, nk_float64 f) noexcept {
        return SetValue(k, NkArchiveValue::FromFloat64(f));
    }

    nk_bool NkArchive::SetString(NkStringView k, NkStringView v) noexcept {
        return SetValue(k, NkArchiveValue::FromString(v));
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : GetValue
    // DESCRIPTION : Extraction d'une valeur scalaire avec vérification de type
    // -------------------------------------------------------------------------
    nk_bool NkArchive::GetValue(NkStringView key, NkArchiveValue& out) const noexcept {
        const NkArchiveNode* n = FindNode(key);
        if (!n || !n->IsScalar()) {
            return false;
        }
        out = n->value;
        return true;
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : GetBool
    // DESCRIPTION : Lecture booléenne avec coercition depuis string
    // -------------------------------------------------------------------------
    nk_bool NkArchive::GetBool(NkStringView key, nk_bool& out) const noexcept {
        NkArchiveValue v;
        if (!GetValue(key, v)) {
            return false;
        }

        if (v.type == NkArchiveValueType::NK_VALUE_BOOL) {
            out = v.raw.b;
            return true;
        }

        // Coercition depuis string : parsing de "true"/"false" (case-insensitive)
        bool parsed = false;
        if (!v.text.ToBool(parsed)) {
            return false;
        }
        out = parsed;
        return true;
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : GetInt32
    // DESCRIPTION : Wrapper vers GetInt64 avec cast et vérification de bounds
    // -------------------------------------------------------------------------
    nk_bool NkArchive::GetInt32(NkStringView key, nk_int32& out) const noexcept {
        nk_int64 v = 0;
        if (!GetInt64(key, v)) {
            return false;
        }
        // Vérification optionnelle de bounds pour sécurité
        if (v < static_cast<nk_int64>(INT32_MIN) || v > static_cast<nk_int64>(INT32_MAX)) {
            return false;  // Overflow potentiel
        }
        out = static_cast<nk_int32>(v);
        return true;
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : GetInt64
    // DESCRIPTION : Lecture entier signé avec coercition multi-types
    // -------------------------------------------------------------------------
    nk_bool NkArchive::GetInt64(NkStringView key, nk_int64& out) const noexcept {
        NkArchiveValue v;
        if (!GetValue(key, v)) {
            return false;
        }

        // Cas direct : type match exact
        if (v.type == NkArchiveValueType::NK_VALUE_INT64) {
            out = v.raw.i;
            return true;
        }

        // Coercition depuis uint64 (avec vérification de signe)
        if (v.type == NkArchiveValueType::NK_VALUE_UINT64) {
            if (v.raw.u > static_cast<nk_uint64>(INT64_MAX)) {
                return false;  // Overflow vers signed
            }
            out = static_cast<nk_int64>(v.raw.u);
            return true;
        }

        // Coercition depuis float64 (troncature, pas d'arrondi)
        if (v.type == NkArchiveValueType::NK_VALUE_FLOAT64) {
            if (v.raw.f < static_cast<nk_float64>(INT64_MIN)
                || v.raw.f > static_cast<nk_float64>(INT64_MAX)) {
                return false;  // Hors bounds
            }
            out = static_cast<nk_int64>(v.raw.f);
            return true;
        }

        // Fallback : parsing depuis la représentation textuelle
        return v.text.ToInt64(out);
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : GetUInt32
    // DESCRIPTION : Wrapper vers GetUInt64 avec cast et vérification de bounds
    // -------------------------------------------------------------------------
    nk_bool NkArchive::GetUInt32(NkStringView key, nk_uint32& out) const noexcept {
        nk_uint64 v = 0;
        if (!GetUInt64(key, v)) {
            return false;
        }
        if (v > static_cast<nk_uint64>(UINT32_MAX)) {
            return false;  // Overflow
        }
        out = static_cast<nk_uint32>(v);
        return true;
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : GetUInt64
    // DESCRIPTION : Lecture entier non-signé avec coercition multi-types
    // -------------------------------------------------------------------------
    nk_bool NkArchive::GetUInt64(NkStringView key, nk_uint64& out) const noexcept {
        NkArchiveValue v;
        if (!GetValue(key, v)) {
            return false;
        }

        // Cas direct : type match exact
        if (v.type == NkArchiveValueType::NK_VALUE_UINT64) {
            out = v.raw.u;
            return true;
        }

        // Coercition depuis int64 (seulement si non-négatif)
        if (v.type == NkArchiveValueType::NK_VALUE_INT64) {
            if (v.raw.i < 0) {
                return false;  // Négatif non convertible en unsigned
            }
            out = static_cast<nk_uint64>(v.raw.i);
            return true;
        }

        // Coercition depuis float64 (troncature, vérification de bounds)
        if (v.type == NkArchiveValueType::NK_VALUE_FLOAT64) {
            if (v.raw.f < 0.0 || v.raw.f > static_cast<nk_float64>(UINT64_MAX)) {
                return false;
            }
            out = static_cast<nk_uint64>(v.raw.f);
            return true;
        }

        // Fallback : parsing depuis la représentation textuelle
        return v.text.ToUInt64(out);
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : GetFloat32
    // DESCRIPTION : Wrapper vers GetFloat64 avec cast (perte de précision possible)
    // -------------------------------------------------------------------------
    nk_bool NkArchive::GetFloat32(NkStringView key, nk_float32& out) const noexcept {
        nk_float64 v = 0.0;
        if (!GetFloat64(key, v)) {
            return false;
        }
        out = static_cast<nk_float32>(v);
        return true;
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : GetFloat64
    // DESCRIPTION : Lecture flottant avec coercition depuis types numériques
    // -------------------------------------------------------------------------
    nk_bool NkArchive::GetFloat64(NkStringView key, nk_float64& out) const noexcept {
        NkArchiveValue v;
        if (!GetValue(key, v)) {
            return false;
        }

        // Cas direct : type match exact
        if (v.type == NkArchiveValueType::NK_VALUE_FLOAT64) {
            out = v.raw.f;
            return true;
        }

        // Coercition depuis int64 (conversion exacte dans la plage représentable)
        if (v.type == NkArchiveValueType::NK_VALUE_INT64) {
            out = static_cast<nk_float64>(v.raw.i);
            return true;
        }

        // Coercition depuis uint64 (attention : perte de précision au-delà de 2^53)
        if (v.type == NkArchiveValueType::NK_VALUE_UINT64) {
            out = static_cast<nk_float64>(v.raw.u);
            return true;
        }

        // Types non-numériques : bool et null ne sont pas convertibles en float
        if (v.type == NkArchiveValueType::NK_VALUE_BOOL
            || v.type == NkArchiveValueType::NK_VALUE_NULL) {
            return false;
        }

        // Fallback : parsing depuis la représentation textuelle
        return v.text.ToDouble(out);
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : GetString
    // DESCRIPTION : Extraction de la représentation textuelle canonique
    // -------------------------------------------------------------------------
    nk_bool NkArchive::GetString(NkStringView key, NkString& out) const noexcept {
        NkArchiveValue v;
        if (!GetValue(key, v)) {
            return false;
        }
        // Retourne toujours le texte canonique, quel que soit le type stocké
        out = v.text;
        return true;
    }


    // =============================================================================
    // NkArchive — OBJETS IMBRIQUÉS
    // =============================================================================

    // -------------------------------------------------------------------------
    // MÉTHODE : SetObject
    // DESCRIPTION : Insertion d'un objet avec copie profonde
    // -------------------------------------------------------------------------
    nk_bool NkArchive::SetObject(NkStringView key, const NkArchive& arc) noexcept {
        if (!IsValidKey(key)) {
            return false;
        }

        nk_size idx = FindIndex(key);

        if (idx != NPOS) {
            // Mise à jour du nœud existant via SetObject (gère cleanup)
            mEntries[idx].node.SetObject(arc);
            return true;
        }

        // Nouvelle entrée avec nœud objet
        NkArchiveEntry e;
        e.key = NkString(key);
        e.node.SetObject(arc);
        mEntries.PushBack(std::move(e));
        return true;
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : GetObject
    // DESCRIPTION : Extraction par copie d'un objet imbriqué
    // -------------------------------------------------------------------------
    nk_bool NkArchive::GetObject(NkStringView key, NkArchive& out) const noexcept {
        const NkArchiveNode* n = FindNode(key);
        if (!n || !n->IsObject()) {
            return false;
        }
        // Copie profonde via NkArchive::operator=
        out = *n->object;
        return true;
    }


    // =============================================================================
    // NkArchive — TABLEAUX
    // =============================================================================

    // -------------------------------------------------------------------------
    // MÉTHODE : SetArray
    // DESCRIPTION : Création d'un tableau de valeurs scalaires
    // -------------------------------------------------------------------------
    nk_bool NkArchive::SetArray(NkStringView key, const NkVector<NkArchiveValue>& arr) noexcept {
        if (!IsValidKey(key)) {
            return false;
        }

        NkArchiveNode node;
        node.MakeArray();

        for (nk_size i = 0; i < arr.Size(); ++i) {
            NkArchiveNode elem(arr[i]);
            node.array.PushBack(std::move(elem));
        }

        return SetNode(key, node);
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : GetArray
    // DESCRIPTION : Extraction d'un tableau en filtrant les scalaires uniquement
    // -------------------------------------------------------------------------
    nk_bool NkArchive::GetArray(NkStringView key, NkVector<NkArchiveValue>& out) const noexcept {
        const NkArchiveNode* n = FindNode(key);
        if (!n || !n->IsArray()) {
            return false;
        }

        out.Clear();
        for (nk_size i = 0; i < n->array.Size(); ++i) {
            if (n->array[i].IsScalar()) {
                out.PushBack(n->array[i].value);
            }
            // Les éléments non-scalaires sont ignorés : filtrage implicite
        }
        return true;
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : SetNodeArray
    // DESCRIPTION : Création d'un tableau de nœuds polymorphes
    // -------------------------------------------------------------------------
    nk_bool NkArchive::SetNodeArray(NkStringView key, const NkVector<NkArchiveNode>& arr) noexcept {
        if (!IsValidKey(key)) {
            return false;
        }

        NkArchiveNode node;
        node.MakeArray();
        node.array = arr;  // Copie sémantique via NkVector::operator=
        return SetNode(key, node);
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : GetNodeArray
    // DESCRIPTION : Extraction d'un tableau de nœuds sans filtrage
    // -------------------------------------------------------------------------
    nk_bool NkArchive::GetNodeArray(NkStringView key, NkVector<NkArchiveNode>& out) const noexcept {
        const NkArchiveNode* n = FindNode(key);
        if (!n || !n->IsArray()) {
            return false;
        }
        out = n->array;  // Copie sémantique
        return true;
    }


    // =============================================================================
    // NkArchive — ACCÈS PAR CHEMIN HIÉRARCHIQUE
    // =============================================================================

    // -------------------------------------------------------------------------
    // MÉTHODE : SetPath
    // DESCRIPTION : Navigation récursive pour insertion avec création auto
    // -------------------------------------------------------------------------
    nk_bool NkArchive::SetPath(NkStringView path, const NkArchiveNode& node) noexcept {
        if (path.Empty()) {
            return false;
        }

        // Recherche du premier séparateur '.'
        nk_size dot = static_cast<nk_size>(-1);
        for (nk_size i = 0; i < path.Length(); ++i) {
            if (path[i] == '.') {
                dot = i;
                break;
            }
        }

        // Cas feuille : pas de séparateur → insertion directe
        if (dot == static_cast<nk_size>(-1)) {
            return SetNode(path, node);
        }

        // Séparation head/tail pour récursion
        NkStringView head = path.SubStr(0, dot);
        NkStringView tail = path.SubStr(dot + 1);

        // Récupération ou création du sous-objet intermédiaire
        NkArchive child;
        const NkArchiveNode* existing = FindNode(head);
        if (existing && existing->IsObject()) {
            // Copie de l'existant pour préservation du contenu
            child = *existing->object;
        }

        // Récursion sur le reste du chemin
        if (!child.SetPath(tail, node)) {
            return false;
        }

        // Mise à jour de l'objet parent avec le sous-objet modifié
        return SetObject(head, child);
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : GetPath
    // DESCRIPTION : Navigation récursive en lecture seule
    // -------------------------------------------------------------------------
    const NkArchiveNode* NkArchive::GetPath(NkStringView path) const noexcept {
        if (path.Empty()) {
            return nullptr;
        }

        // Recherche du premier séparateur '.'
        nk_size dot = static_cast<nk_size>(-1);
        for (nk_size i = 0; i < path.Length(); ++i) {
            if (path[i] == '.') {
                dot = i;
                break;
            }
        }

        // Cas feuille : retour direct via FindNode
        if (dot == static_cast<nk_size>(-1)) {
            return FindNode(path);
        }

        // Séparation head/tail pour récursion
        NkStringView head = path.SubStr(0, dot);
        NkStringView tail = path.SubStr(dot + 1);

        // Navigation vers le sous-objet
        const NkArchiveNode* n = FindNode(head);
        if (!n || !n->IsObject()) {
            return nullptr;  // Chaîne rompue : chemin invalide
        }

        // Récursion sur le reste du chemin
        return n->object->GetPath(tail);
    }


    // =============================================================================
    // NkArchive — MÉTA-DONNÉES
    // =============================================================================

    // -------------------------------------------------------------------------
    // MÉTHODE : SetMeta
    // DESCRIPTION : Stockage sous "__meta__.key" via SetPath
    // -------------------------------------------------------------------------
    nk_bool NkArchive::SetMeta(NkStringView metaKey, NkStringView value) noexcept {
        // Construction du chemin complet "__meta__.metaKey"
        NkString path("__meta__.");
        path.Append(metaKey);
        // Création d'un nœud scalaire string pour la valeur
        return SetPath(path.View(), NkArchiveNode(NkArchiveValue::FromString(value)));
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : GetMeta
    // DESCRIPTION : Lecture depuis "__meta__.key" via GetPath
    // -------------------------------------------------------------------------
    nk_bool NkArchive::GetMeta(NkStringView metaKey, NkString& out) const noexcept {
        NkString path("__meta__.");
        path.Append(metaKey);
        const NkArchiveNode* n = GetPath(path.View());
        if (!n || !n->IsScalar()) {
            return false;
        }
        out = n->value.text;
        return true;
    }


    // =============================================================================
    // NkArchive — GESTION GÉNÉRALE
    // =============================================================================

    // -------------------------------------------------------------------------
    // MÉTHODE : Has
    // DESCRIPTION : Vérification de présence via FindIndex
    // -------------------------------------------------------------------------
    nk_bool NkArchive::Has(NkStringView key) const noexcept {
        return FindIndex(key) != NPOS;
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : Remove
    // DESCRIPTION : Suppression avec libération automatique des ressources
    // -------------------------------------------------------------------------
    nk_bool NkArchive::Remove(NkStringView key) noexcept {
        nk_size idx = FindIndex(key);
        if (idx == NPOS) {
            return false;
        }
        // Erase appelle automatiquement le destructeur de NkArchiveEntry
        // qui libère l'objet possédé si kind == NK_NODE_OBJECT
        mEntries.Erase(mEntries.begin() + static_cast<ptrdiff_t>(idx));
        return true;
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : Clear
    // DESCRIPTION : Vidage complet avec libération de toutes les ressources
    // -------------------------------------------------------------------------
    void NkArchive::Clear() noexcept {
        mEntries.Clear();
    }

    // -------------------------------------------------------------------------
    // MÉTHODE : Merge
    // DESCRIPTION : Fusion avec contrôle d'écrasement des clés existantes
    // -------------------------------------------------------------------------
    void NkArchive::Merge(const NkArchive& other, bool overwrite) noexcept {
        for (nk_size i = 0; i < other.mEntries.Size(); ++i) {
            const NkArchiveEntry& e = other.mEntries[i];
            // Écrasement conditionnel selon le flag
            if (overwrite || !Has(e.key.View())) {
                SetNode(e.key.View(), e.node);
            }
        }
    }


} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================