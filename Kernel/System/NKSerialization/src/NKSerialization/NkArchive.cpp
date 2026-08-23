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
// License : Proprietary - All Rights Reserved (see LICENSE)
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
	static void CloneNode(NkArchiveNode &dst, const NkArchiveNode &src) noexcept {
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

		// NOTE : la trivia n'est PAS copiée ici. CloneNode est une fonction libre,
		// et CopyTriviaFrom() est privée ; les deux appelants (constructeur de
		// copie et affectation par copie) la copient eux-mêmes, juste après.
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
		FreeTrivia();
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : Constructeur de copie
	// DESCRIPTION : Duplication profonde via CloneNode helper
	// -------------------------------------------------------------------------
	NkArchiveNode::NkArchiveNode(const NkArchiveNode &o) noexcept {
		CloneNode(*this, o);
		// Trivia : DUPLIQUÉE, jamais partagée. C'est un pointeur possédant, au
		// même titre que `object` — le partager provoquerait une double
		// libération, et modifier la mise en forme d'une copie remonterait dans
		// l'original.
		CopyTriviaFrom(o);
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : Constructeur de move
	// DESCRIPTION : Transfert de propriété avec réinitialisation de la source
	// -------------------------------------------------------------------------
	NkArchiveNode::NkArchiveNode(NkArchiveNode &&o) noexcept
		: kind(o.kind), value(traits::NkMove(o.value)), object(o.object), array(traits::NkMove(o.array)),
		  mTrivia(o.mTrivia) {
		// Réinitialisation de la source pour sécurité post-move
		o.object = nullptr;
		o.mTrivia = nullptr;
		o.kind = NkNodeKind::NK_NODE_SCALAR;
		// array est déjà vidé par traits::NkMove, value est dans état valide
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : Opérateur d'affectation par copie
	// DESCRIPTION : Copie profonde avec auto-assignation guard et cleanup
	// -------------------------------------------------------------------------
	NkArchiveNode &NkArchiveNode::operator=(const NkArchiveNode &o) noexcept {
		if (this != &o) {
			FreeObject();
			array.Clear();
			CloneNode(*this, o);
			CopyTriviaFrom(o); // libère l'ancien bloc avant de dupliquer
		}
		return *this;
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : Opérateur d'affectation par move
	// DESCRIPTION : Transfert de propriété avec auto-assignation guard
	// -------------------------------------------------------------------------
	NkArchiveNode &NkArchiveNode::operator=(NkArchiveNode &&o) noexcept {
		if (this != &o) {
			FreeObject();
			array.Clear();
			FreeTrivia();
			kind = o.kind;
			value = traits::NkMove(o.value);
			object = o.object;
			array = traits::NkMove(o.array);
			mTrivia = o.mTrivia;
			// Réinitialisation de la source
			o.object = nullptr;
			o.mTrivia = nullptr;
			o.kind = NkNodeKind::NK_NODE_SCALAR;
		}
		return *this;
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : SetObject
	// DESCRIPTION : Configure ce nœud comme objet avec copie profonde
	// -------------------------------------------------------------------------
	void NkArchiveNode::SetObject(const NkArchive &arc) noexcept {
		FreeObject();
		array.Clear();
		kind = NkNodeKind::NK_NODE_OBJECT;
		object = new NkArchive(arc);
	}


	// =============================================================================
	// NkArchiveNode — TRIVIA (mise en forme d'origine, facultative)
	// =============================================================================
	// Bloc alloué à la demande : un nœud fabriqué par le code n'alloue rien.

	// -------------------------------------------------------------------------
	// MÉTHODE : FreeTrivia
	// DESCRIPTION : Libère le bloc de trivia possédé si présent
	// -------------------------------------------------------------------------
	void NkArchiveNode::FreeTrivia() noexcept {
		delete mTrivia;
		mTrivia = nullptr;
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : CopyTriviaFrom
	// DESCRIPTION : Duplique le bloc de trivia d'un autre nœud (jamais partagé)
	// -------------------------------------------------------------------------
	void NkArchiveNode::CopyTriviaFrom(const NkArchiveNode &o) noexcept {
		FreeTrivia();
		if (o.mTrivia) {
			mTrivia = new NkArchiveTrivia(*o.mTrivia);
		}
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : EnsureTrivia
	// DESCRIPTION : Alloue le bloc à la première demande
	// -------------------------------------------------------------------------
	NkArchiveTrivia &NkArchiveNode::EnsureTrivia() noexcept {
		if (!mTrivia) {
			mTrivia = new NkArchiveTrivia();
		}
		return *mTrivia;
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : ClearTrivia
	// -------------------------------------------------------------------------
	void NkArchiveNode::ClearTrivia() noexcept {
		FreeTrivia();
	}

	// -------------------------------------------------------------------------
	// MÉTHODES : Trivia — accès par champ
	// -------------------------------------------------------------------------
	void NkArchiveNode::SetLeadingTrivia(NkStringView t) noexcept {
		EnsureTrivia().leading = NkString(t);
	}

	void NkArchiveNode::SetTrailingTrivia(NkStringView t) noexcept {
		EnsureTrivia().trailing = NkString(t);
	}

	NkStringView NkArchiveNode::LeadingTrivia() const noexcept {
		return mTrivia ? mTrivia->leading.View() : NkStringView();
	}

	NkStringView NkArchiveNode::TrailingTrivia() const noexcept {
		return mTrivia ? mTrivia->trailing.View() : NkStringView();
	}

	void NkArchiveNode::SetSourceOrder(nk_int32 rank) noexcept {
		if (rank < 0) {
			// Effacer un rang ne doit pas allouer un bloc pour rien.
			if (mTrivia) {
				mTrivia->sourceOrder = -1;
			}
			return;
		}
		EnsureTrivia().sourceOrder = rank;
	}

	nk_int32 NkArchiveNode::SourceOrder() const noexcept {
		return mTrivia ? mTrivia->sourceOrder : -1;
	}

	void NkArchiveNode::SetSourceLine(nk_int32 line) noexcept {
		if (line < 0) {
			// Effacer une ligne ne doit pas allouer un bloc pour rien.
			if (mTrivia) {
				mTrivia->sourceLine = -1;
			}
			return;
		}
		EnsureTrivia().sourceLine = line;
	}

	nk_int32 NkArchiveNode::SourceLine() const noexcept {
		return mTrivia ? mTrivia->sourceLine : -1;
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : SetLiteral
	// DESCRIPTION : Pose le lexème d'origine ET le texte canonique qu'il dénotait
	// -------------------------------------------------------------------------
	// `literalOf` est renseigné ICI, depuis la valeur courante — jamais par
	// l'appelant. C'est ce qui rend le garde-fou anti-périmé impossible à oublier :
	// il n'y a pas d'appel où on pourrait « ne pas le faire ».
	void NkArchiveNode::SetLiteral(NkStringView literal) noexcept {
		NkArchiveTrivia &t = EnsureTrivia();
		t.literal = NkString(literal);
		t.literalOf = NkString(CanonicalLexeme());
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : HasUsableLiteral
	// DESCRIPTION : Un littéral n'est utilisable que s'il dénote ENCORE la valeur
	// -------------------------------------------------------------------------
	bool NkArchiveNode::HasUsableLiteral() const noexcept {
		if (!mTrivia || mTrivia->literal.Empty()) {
			return false;
		}
		if (kind != NkNodeKind::NK_NODE_SCALAR) {
			return false;
		}
		return mTrivia->literalOf.View() == CanonicalLexeme();
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : CanonicalLexeme
	// DESCRIPTION : Ce que l'archive écrirait sans rien savoir du fichier
	// -------------------------------------------------------------------------
	NkStringView NkArchiveNode::CanonicalLexeme() const noexcept {
		if (kind != NkNodeKind::NK_NODE_SCALAR) {
			return NkStringView();
		}
		if (value.type == NkArchiveValueType::NK_VALUE_NULL) {
			// `text` est vide pour un null : la forme canonique est le mot-clé.
			// Sans ce cas, un null s'imprimerait vide — exactement le défaut de
			// NkGValue::raw qu'on ne veut pas reproduire.
			return NkStringView("null");
		}
		return value.text.View();
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : Lexeme
	// DESCRIPTION : L'UNIQUE point d'impression d'un scalaire
	// -------------------------------------------------------------------------
	NkStringView NkArchiveNode::Lexeme() const noexcept {
		if (HasUsableLiteral()) {
			return mTrivia->literal.View();
		}
		return CanonicalLexeme();
	}

	// =============================================================================
	// NkArchive — BIG FIVE EXPLICITE (à cause du pointeur possédant mTrivia)
	// =============================================================================
	// Ces cinq méthodes étaient `= default` jusqu'au 2026-08-21. L'archive ne
	// contenait qu'un NkVector, dont la sémantique de valeur suffisait. Depuis
	// qu'elle porte un pointeur POSSÉDANT (mTrivia), un `= default` copierait le
	// pointeur : deux archives libéreraient le même bloc.

	NkArchive::NkArchive(const NkArchive &o) noexcept : mEntries(o.mEntries) {
		if (o.mTrivia) {
			mTrivia = new NkArchiveTrivia(*o.mTrivia);
		}
	}

	NkArchive::NkArchive(NkArchive &&o) noexcept : mEntries(traits::NkMove(o.mEntries)), mTrivia(o.mTrivia) {
		o.mTrivia = nullptr;
	}

	NkArchive &NkArchive::operator=(const NkArchive &o) noexcept {
		if (this != &o) {
			FreeTrivia();
			mEntries = o.mEntries;
			if (o.mTrivia) {
				mTrivia = new NkArchiveTrivia(*o.mTrivia);
			}
		}
		return *this;
	}

	NkArchive &NkArchive::operator=(NkArchive &&o) noexcept {
		if (this != &o) {
			FreeTrivia();
			mEntries = traits::NkMove(o.mEntries);
			mTrivia = o.mTrivia;
			o.mTrivia = nullptr;
		}
		return *this;
	}

	NkArchive::~NkArchive() noexcept {
		FreeTrivia();
	}

	// =============================================================================
	// NkArchive — TRIVIA (en-tête / pied, et accès par clé)
	// =============================================================================

	void NkArchive::FreeTrivia() noexcept {
		delete mTrivia;
		mTrivia = nullptr;
	}

	NkArchiveTrivia &NkArchive::EnsureTrivia() noexcept {
		if (!mTrivia) {
			mTrivia = new NkArchiveTrivia();
		}
		return *mTrivia;
	}

	void NkArchive::ClearTrivia() noexcept {
		FreeTrivia();
	}

	void NkArchive::SetHeaderTrivia(NkStringView t) noexcept {
		EnsureTrivia().leading = NkString(t);
	}

	void NkArchive::SetFooterTrivia(NkStringView t) noexcept {
		EnsureTrivia().trailing = NkString(t);
	}

	NkStringView NkArchive::HeaderTrivia() const noexcept {
		return mTrivia ? mTrivia->leading.View() : NkStringView();
	}

	NkStringView NkArchive::FooterTrivia() const noexcept {
		return mTrivia ? mTrivia->trailing.View() : NkStringView();
	}

	nk_bool NkArchive::SetLeadingTrivia(NkStringView key, NkStringView t) noexcept {
		NkArchiveNode *n = FindNode(key);
		if (!n) {
			return false;
		}
		n->SetLeadingTrivia(t);
		return true;
	}

	nk_bool NkArchive::SetTrailingTrivia(NkStringView key, NkStringView t) noexcept {
		NkArchiveNode *n = FindNode(key);
		if (!n) {
			return false;
		}
		n->SetTrailingTrivia(t);
		return true;
	}

	nk_bool NkArchive::SetLiteral(NkStringView key, NkStringView literal) noexcept {
		NkArchiveNode *n = FindNode(key);
		if (!n) {
			return false;
		}
		n->SetLiteral(literal);
		return true;
	}

	nk_bool NkArchive::SetSourceOrder(NkStringView key, nk_int32 rank) noexcept {
		NkArchiveNode *n = FindNode(key);
		if (!n) {
			return false;
		}
		n->SetSourceOrder(rank);
		return true;
	}

	nk_bool NkArchive::SetSourceLine(NkStringView key, nk_int32 line) noexcept {
		NkArchiveNode *n = FindNode(key);
		if (!n) {
			return false;
		}
		n->SetSourceLine(line);
		return true;
	}

	nk_int32 NkArchive::GetSourceLine(NkStringView key) const noexcept {
		const NkArchiveNode *n = FindNode(key);
		return n ? n->SourceLine() : -1;
	}

	nk_int32 NkArchive::GetSourceOrder(NkStringView key) const noexcept {
		const NkArchiveNode *n = FindNode(key);
		return n ? n->SourceOrder() : -1;
	}

	NkStringView NkArchive::Lexeme(NkStringView key) const noexcept {
		const NkArchiveNode *n = FindNode(key);
		return n ? n->Lexeme() : NkStringView();
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : SortBySourceOrder
	// DESCRIPTION : Remet les entrées dans l'ordre du fichier d'origine
	// -------------------------------------------------------------------------
	// Tri par INSERTION, donc STABLE — et la stabilité n'est pas un détail : les
	// entrées sans rang (une propriété ajoutée après lecture) doivent conserver
	// leur ordre relatif et finir à la fin, pas se retrouver à un endroit
	// arbitraire. Insertion et non tri rapide : n est petit (< 100 clés par objet,
	// hypothèse déjà posée par la recherche linéaire de FindIndex).
	void NkArchive::SortBySourceOrder(bool recursive) noexcept {
		const nk_size count = mEntries.Size();

		for (nk_size i = 1; i < count; ++i) {
			// Rang de l'entrée à replacer ; -1 (inconnu) compte comme +infini.
			const nk_int32 rankI = mEntries[i].node.SourceOrder();
			if (rankI < 0) {
				continue; // sans rang : reste où elle est, donc après les rangées
			}

			nk_size j = i;
			while (j > 0) {
				const nk_int32 rankPrev = mEntries[j - 1].node.SourceOrder();
				const bool prevIsLater = (rankPrev < 0) || (rankPrev > rankI);
				if (!prevIsLater) {
					break;
				}
				NkArchiveEntry tmp = traits::NkMove(mEntries[j - 1]);
				mEntries[j - 1] = traits::NkMove(mEntries[j]);
				mEntries[j] = traits::NkMove(tmp);
				--j;
			}
		}

		if (!recursive) {
			return;
		}

		for (nk_size i = 0; i < mEntries.Size(); ++i) {
			SortNodeBySourceOrder(mEntries[i].node);
		}
	}

	// -------------------------------------------------------------------------
	// MÉTHODE PRIVÉE : SortNodeBySourceOrder
	// DESCRIPTION : Descend le tri dans les objets et les tableaux
	// -------------------------------------------------------------------------
	void NkArchive::SortNodeBySourceOrder(NkArchiveNode &node) noexcept {
		if (node.IsObject()) {
			node.object->SortBySourceOrder(true);
			return;
		}
		if (node.IsArray()) {
			// L'ordre d'un tableau est intrinsèque : on ne le trie PAS, on
			// descend seulement dans ses éléments.
			for (nk_size i = 0; i < node.array.Size(); ++i) {
				SortNodeBySourceOrder(node.array[i]);
			}
		}
	}

	// -------------------------------------------------------------------------
	// MÉTHODE PRIVÉE : AdoptNodeFormatting
	// DESCRIPTION : Greffe la mise en forme d'un nœud source sur un nœud cible
	// -------------------------------------------------------------------------
	void NkArchive::AdoptNodeFormatting(NkArchiveNode &dst, const NkArchiveNode &src) noexcept {
		const NkArchiveTrivia *st = src.Trivia();

		if (st) {
			// Un commentaire appartient à la LIGNE, pas à la valeur : il survit à
			// une modification de la valeur. L'ordre aussi.
			if (!st->leading.Empty()) {
				dst.SetLeadingTrivia(st->leading.View());
			}
			if (!st->trailing.Empty()) {
				dst.SetTrailingTrivia(st->trailing.View());
			}
			if (st->sourceOrder >= 0) {
				dst.SetSourceOrder(st->sourceOrder);
			}
			if (st->sourceLine >= 0) {
				dst.SetSourceLine(st->sourceLine);
			}

			// La forme littérale, elle, appartient à LA VALEUR. On ne la reprend
			// que si la valeur n'a pas bougé : réimprimer 0.50 sur une valeur
			// devenue 0.75 ne serait pas « préserver la mise en forme », ce serait
			// PERDRE la modification.
			if (!st->literal.Empty() && dst.IsScalar() && src.IsScalar() && dst.value.type == src.value.type &&
				dst.value.text == src.value.text) {
				dst.SetLiteral(st->literal.View());
			}
		}

		if (dst.IsObject() && src.IsObject()) {
			dst.object->AdoptFormattingNoSort(*src.object);
			return;
		}

		if (dst.IsArray() && src.IsArray()) {
			// Appariement par INDICE : un tableau n'a pas de clé.
			const nk_size n = (dst.array.Size() < src.array.Size()) ? dst.array.Size() : src.array.Size();
			for (nk_size i = 0; i < n; ++i) {
				AdoptNodeFormatting(dst.array[i], src.array[i]);
			}
		}
	}

	// -------------------------------------------------------------------------
	// MÉTHODE PRIVÉE : AdoptFormattingNoSort
	// DESCRIPTION : Le corps récursif de AdoptFormatting, sans le tri final
	// -------------------------------------------------------------------------
	void NkArchive::AdoptFormattingNoSort(const NkArchive &source) noexcept {
		const NkArchiveTrivia *st = source.Trivia();
		if (st) {
			if (!st->leading.Empty()) {
				SetHeaderTrivia(st->leading.View());
			}
			if (!st->trailing.Empty()) {
				SetFooterTrivia(st->trailing.View());
			}
		}

		for (nk_size i = 0; i < mEntries.Size(); ++i) {
			const NkArchiveNode *sn = source.FindNode(mEntries[i].key.View());
			if (!sn) {
				// Clé absente de la source : elle est neuve, elle n'a pas de
				// passé. On ne lui invente ni commentaire ni rang — elle ira
				// donc à la fin, ce qui est le comportement voulu.
				continue;
			}
			AdoptNodeFormatting(mEntries[i].node, *sn);
		}
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : AdoptFormatting
	// DESCRIPTION : Greffe complète + remise en ordre du fichier
	// -------------------------------------------------------------------------
	void NkArchive::AdoptFormatting(const NkArchive &source) noexcept {
		AdoptFormattingNoSort(source);
		SortBySourceOrder(true);
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
	nk_bool NkArchive::SetNode(NkStringView key, const NkArchiveNode &node) noexcept {
		if (!IsValidKey(key)) {
			return false;
		}

		nk_size idx = FindIndex(key);

		if (idx != NPOS) {
			// Mise à jour de l'entrée existante.
			//
			// ⚠️ Un commentaire appartient à la LIGNE, pas à la valeur : écrire
			// `width = 400` là où il y avait `width = 320  # largeur du panneau`
			// ne doit pas faire disparaître le commentaire. On conserve donc la
			// trivia de l'entrée quand le nœud entrant n'en apporte pas.
			// Le littéral périmé, lui, se désarme tout seul (voir literalOf).
			if (!node.HasTrivia() && mEntries[idx].node.HasTrivia()) {
				NkArchiveTrivia kept = *mEntries[idx].node.Trivia();
				mEntries[idx].node = node;
				mEntries[idx].node.EnsureTrivia() = kept;
			} else {
				mEntries[idx].node = node;
			}
			return true;
		}

		// Création d'une nouvelle entrée
		NkArchiveEntry e;
		e.key = NkString(key);
		e.node = node;
		mEntries.PushBack(traits::NkMove(e));
		return true;
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : GetNode
	// DESCRIPTION : Récupération par copie d'un nœud existant
	// -------------------------------------------------------------------------
	nk_bool NkArchive::GetNode(NkStringView key, NkArchiveNode &out) const noexcept {
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
	const NkArchiveNode *NkArchive::FindNode(NkStringView key) const noexcept {
		nk_size idx = FindIndex(key);
		return (idx != NPOS) ? &mEntries[idx].node : nullptr;
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : FindNode (mutable)
	// DESCRIPTION : Accès en écriture par pointeur sans copie
	// -------------------------------------------------------------------------
	NkArchiveNode *NkArchive::FindNode(NkStringView key) noexcept {
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
	nk_bool NkArchive::SetValue(NkStringView key, const NkArchiveValue &v) noexcept {
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
	nk_bool NkArchive::GetValue(NkStringView key, NkArchiveValue &out) const noexcept {
		const NkArchiveNode *n = FindNode(key);
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
	nk_bool NkArchive::GetBool(NkStringView key, nk_bool &out) const noexcept {
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
	nk_bool NkArchive::GetInt32(NkStringView key, nk_int32 &out) const noexcept {
		nk_int64 v = 0;
		if (!GetInt64(key, v)) {
			return false;
		}
		// Vérification optionnelle de bounds pour sécurité
		if (v < static_cast<nk_int64>(INT32_MIN) || v > static_cast<nk_int64>(INT32_MAX)) {
			return false; // Overflow potentiel
		}
		out = static_cast<nk_int32>(v);
		return true;
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : GetInt64
	// DESCRIPTION : Lecture entier signé avec coercition multi-types
	// -------------------------------------------------------------------------
	nk_bool NkArchive::GetInt64(NkStringView key, nk_int64 &out) const noexcept {
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
				return false; // Overflow vers signed
			}
			out = static_cast<nk_int64>(v.raw.u);
			return true;
		}

		// Coercition depuis float64 (troncature, pas d'arrondi)
		if (v.type == NkArchiveValueType::NK_VALUE_FLOAT64) {
			if (v.raw.f < static_cast<nk_float64>(INT64_MIN) || v.raw.f > static_cast<nk_float64>(INT64_MAX)) {
				return false; // Hors bounds
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
	nk_bool NkArchive::GetUInt32(NkStringView key, nk_uint32 &out) const noexcept {
		nk_uint64 v = 0;
		if (!GetUInt64(key, v)) {
			return false;
		}
		if (v > static_cast<nk_uint64>(UINT32_MAX)) {
			return false; // Overflow
		}
		out = static_cast<nk_uint32>(v);
		return true;
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : GetUInt64
	// DESCRIPTION : Lecture entier non-signé avec coercition multi-types
	// -------------------------------------------------------------------------
	nk_bool NkArchive::GetUInt64(NkStringView key, nk_uint64 &out) const noexcept {
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
				return false; // Négatif non convertible en unsigned
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
	nk_bool NkArchive::GetFloat32(NkStringView key, nk_float32 &out) const noexcept {
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
	nk_bool NkArchive::GetFloat64(NkStringView key, nk_float64 &out) const noexcept {
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
		if (v.type == NkArchiveValueType::NK_VALUE_BOOL || v.type == NkArchiveValueType::NK_VALUE_NULL) {
			return false;
		}

		// Fallback : parsing depuis la représentation textuelle
		return v.text.ToDouble(out);
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : GetString
	// DESCRIPTION : Extraction de la représentation textuelle canonique
	// -------------------------------------------------------------------------
	nk_bool NkArchive::GetString(NkStringView key, NkString &out) const noexcept {
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
	nk_bool NkArchive::SetObject(NkStringView key, const NkArchive &arc) noexcept {
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
		mEntries.PushBack(traits::NkMove(e));
		return true;
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : GetObject
	// DESCRIPTION : Extraction par copie d'un objet imbriqué
	// -------------------------------------------------------------------------
	nk_bool NkArchive::GetObject(NkStringView key, NkArchive &out) const noexcept {
		const NkArchiveNode *n = FindNode(key);
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
	nk_bool NkArchive::SetArray(NkStringView key, const NkVector<NkArchiveValue> &arr) noexcept {
		if (!IsValidKey(key)) {
			return false;
		}

		NkArchiveNode node;
		node.MakeArray();

		for (nk_size i = 0; i < arr.Size(); ++i) {
			NkArchiveNode elem(arr[i]);
			node.array.PushBack(traits::NkMove(elem));
		}

		return SetNode(key, node);
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : GetArray
	// DESCRIPTION : Extraction d'un tableau en filtrant les scalaires uniquement
	// -------------------------------------------------------------------------
	nk_bool NkArchive::GetArray(NkStringView key, NkVector<NkArchiveValue> &out) const noexcept {
		const NkArchiveNode *n = FindNode(key);
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
	// MÉTHODE : SetObjectArray
	// DESCRIPTION : Création d'un tableau d'OBJETS : chaque NkArchive de `arr`
	//               devient un nœud objet du tableau. Symétrique de GetObjectArray.
	//               Permet d'écrire "[ {..}, {..} ]" (conteneurs d'objets réfléchis,
	//               entités d'une scène…). Copie profonde de chaque archive.
	// -------------------------------------------------------------------------
	nk_bool NkArchive::SetObjectArray(NkStringView key, const NkVector<NkArchive> &arr) noexcept {
		if (!IsValidKey(key)) {
			return false;
		}

		NkArchiveNode node;
		node.MakeArray();

		for (nk_size i = 0; i < arr.Size(); ++i) {
			NkArchiveNode elem;
			elem.SetObject(arr[i]); // copie profonde de l'archive -> nœud objet
			node.array.PushBack(traits::NkMove(elem));
		}

		return SetNode(key, node);
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : GetObjectArray
	// DESCRIPTION : Extraction des éléments OBJET d'un tableau, chacun en NkArchive
	//               (copie profonde). Complément de GetArray (scalaires). Permet de
	//               lire "[ {..}, {..} ]" (niveaux, prefabs, scènes…).
	// -------------------------------------------------------------------------
	nk_bool NkArchive::GetObjectArray(NkStringView key, NkVector<NkArchive> &out) const noexcept {
		const NkArchiveNode *n = FindNode(key);
		if (!n || !n->IsArray()) {
			return false;
		}

		out.Clear();
		for (nk_size i = 0; i < n->array.Size(); ++i) {
			if (n->array[i].IsObject()) {
				out.PushBack(*n->array[i].object); // copie profonde via copy-ctor NkArchive
			}
			// Les éléments non-objets sont ignorés.
		}
		return true;
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : SetNodeArray
	// DESCRIPTION : Création d'un tableau de nœuds polymorphes
	// -------------------------------------------------------------------------
	nk_bool NkArchive::SetNodeArray(NkStringView key, const NkVector<NkArchiveNode> &arr) noexcept {
		if (!IsValidKey(key)) {
			return false;
		}

		NkArchiveNode node;
		node.MakeArray();
		node.array = arr; // Copie sémantique via NkVector::operator=
		return SetNode(key, node);
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : GetNodeArray
	// DESCRIPTION : Extraction d'un tableau de nœuds sans filtrage
	// -------------------------------------------------------------------------
	nk_bool NkArchive::GetNodeArray(NkStringView key, NkVector<NkArchiveNode> &out) const noexcept {
		const NkArchiveNode *n = FindNode(key);
		if (!n || !n->IsArray()) {
			return false;
		}
		out = n->array; // Copie sémantique
		return true;
	}

	// =============================================================================
	// NkArchive — ACCÈS PAR CHEMIN HIÉRARCHIQUE
	// =============================================================================

	// -------------------------------------------------------------------------
	// MÉTHODE : SetPath
	// DESCRIPTION : Navigation récursive pour insertion avec création auto
	// -------------------------------------------------------------------------
	nk_bool NkArchive::SetPath(NkStringView path, const NkArchiveNode &node) noexcept {
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
		const NkArchiveNode *existing = FindNode(head);
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
	const NkArchiveNode *NkArchive::GetPath(NkStringView path) const noexcept {
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
		const NkArchiveNode *n = FindNode(head);
		if (!n || !n->IsObject()) {
			return nullptr; // Chaîne rompue : chemin invalide
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
	nk_bool NkArchive::GetMeta(NkStringView metaKey, NkString &out) const noexcept {
		NkString path("__meta__.");
		path.Append(metaKey);
		const NkArchiveNode *n = GetPath(path.View());
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
	void NkArchive::Merge(const NkArchive &other, bool overwrite) noexcept {
		for (nk_size i = 0; i < other.mEntries.Size(); ++i) {
			const NkArchiveEntry &e = other.mEntries[i];
			// Écrasement conditionnel selon le flag
			if (overwrite || !Has(e.key.View())) {
				SetNode(e.key.View(), e.node);
			}
		}
	}

} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - All Rights Reserved (see LICENSE)
// ============================================================